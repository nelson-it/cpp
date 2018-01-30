#ifdef PTHREAD
#include <pthread.h>
#endif


#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#if ! defined(__MINGW32__) && ! defined(__CYGWIN__)
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>

#if defined (MACOS)
extern char **environ;
#endif

#if defined (LINUX)
#include <linux/limits.h>
#endif

#else
#include <utils/tostring.h>
#endif

#include <signal.h>
#include <fcntl.h>
#include <string.h>

#include <utils/cslist.h>

#include "process.h"

#if defined(__MINGW32__) || defined(__CYGWIN__)

void *ProcessWaitStop(void *param)
{
    Process *p = (Process*)param;
    p->wait();
    pthread_exit(NULL);
    return NULL;
}

#endif

Process::~Process()
{
#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( waitidvalid )
    {
        Pthread_join(waitid,NULL);
        waitidvalid = 0;
    }
#else
    if ( file >=0 )
    {
        close(file);
        file = -1;
    }
#endif
    stop();
}

int Process::start(const char *cmd, const char *logfile, const char *workdir, const char *logdir, const char *extrapath, int nomask )
{
    CsList cmd_list(cmd, ' ');
    return start(cmd_list, logfile, workdir, logdir, extrapath, nomask);
}

int Process::start(CsList cmd_list, const char *p_logfile, const char *workdir, const char *logdir, const char *extrapath, int nomask )
{
    this->status = -1;
    std::string logfile = "";
    int pipe = 0;

    if ( p_logfile != NULL )
    {
        logfile = p_logfile;
        if ( logfile.substr(0,4) == std::string("pipe") )
        {
            pipe = 1;
            if ( logfile.size() > 4 ) logfile = logfile.substr(5);
            else logfile = "";
        }
    }
    #if defined(__MINGW32__) || defined(__CYGWIN__)

	SECURITY_ATTRIBUTES psa;
	SECURITY_ATTRIBUTES tsa;
	char path[1024];
	unsigned int i;

	ZeroMemory( &si, sizeof(si) );
	ZeroMemory( &pi, sizeof(pi) );
	pi.hProcess = INVALID_HANDLE_VALUE;
	have_logfile = 0;
	have_pipe = 0;

	TCHAR actdir[MAX_PATH];
	DWORD dwRet;

	if ( waitidvalid )
	    Pthread_join(waitid,NULL);
	waitidvalid = 0;

	if ( workdir != NULL && *workdir != '\0' )
	{
		dwRet = GetCurrentDirectory(MAX_PATH, actdir);

	    if ( dwRet == 0 || dwRet > MAX_PATH )
	        msg.pwarning(E_FOLDER, "kann aktuellen Ordner nicht ermitteln");

	    if ( SetCurrentDirectory(workdir) == 0 )
		{
			msg.perror(E_FOLDER, "kann nicht in Ordner <%s> wechseln", workdir);
			return 0;
		}
	}

	if ( extrapath != NULL && *extrapath != '\0' )
	{
		unsigned int i;
		i = GetEnvironmentVariable("PATH", path, sizeof(path));
		if ( i > sizeof(path) )
		{
			msg.perror(E_PATH, "kann $PATH nicht mit <%s> erweitern",extrapath);
			return 0;
		}

		path[i] = '\0';
		SetEnvironmentVariable("PATH", ((std::string)path + ";" + (std::string)extrapath).c_str());
	}

	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	si.hStdInput = INVALID_HANDLE_VALUE;
	si.hStdOutput = INVALID_HANDLE_VALUE;
	si.hStdError = INVALID_HANDLE_VALUE;

    if ( pipe )
    {
        std::string lf;

        if ( ! CreatePipe(&si.hStdInput, &pipew, NULL, 0) )
        {
            msg.perror(E_PIPE, "kann Pipe nicht erzeugen");

            if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                    msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

            return 0;
        }

        if ( ! SetHandleInformation(si.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) )
        {
            msg.perror(E_PIPE, "kann Handleinformationen nicht setzen");

            if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                    msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

            return 0;
        }

        if ( ! CreatePipe(&piper, &si.hStdOutput, NULL, 0) )
        {
            msg.perror(E_PIPE, "kann Pipe nicht erzeugen");

            if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                    msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

            return 0;
        }

        if ( ! SetHandleInformation(si.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) )
        {
            msg.perror(E_PIPE, "kann Handleinformationen nicht setzen");

            if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                    msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

            return 0;
        }

        if ( logfile != "" )
        {
            si.hStdError = si.hStdOutput;
        }

        have_pipe = 1;
    }

    if ( logfile != "" )
    {
        std::string lf;

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        if ( logdir != NULL && logfile[0] != '\\' )
            lf = (std::string)logdir + "\\" + logfile;
        else
            lf = logfile;

        si.hStdError = CreateFile(lf.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                &sa,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);

        if ( ! pipe )
            si.hStdOutput  = si.hStdError;

        if ( si.hStdError == INVALID_HANDLE_VALUE )
        {
            msg.perror(E_LOGFILE, "konnte logfile <%s> nicht öffnen", logfile.c_str());
            SetEnvironmentVariable("PATH", path);

            if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                    msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

            return 0;
        }
        have_logfile = 1;
    }
    else if ( ! pipe )
    {
        si.hStdOutput  = GetStdHandle(STD_ERROR_HANDLE);
        si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
	}

    STARTUPINFOEX ssi;
    SIZE_T size = 0;
    BOOL fSuccess;
    BOOL fInitialized = FALSE;

    ssi.lpAttributeList = NULL;
    fSuccess = InitializeProcThreadAttributeList(NULL, 1, 0, &size) || GetLastError() == ERROR_INSUFFICIENT_BUFFER;
    if (fSuccess) fSuccess = ( ssi.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST) (HeapAlloc(GetProcessHeap(), 0, size)) ) != NULL;
    if (fSuccess) fSuccess = InitializeProcThreadAttributeList(ssi.lpAttributeList, 1, 0, &size);
    if (fSuccess)
    {
        int num = 0;
        fInitialized = TRUE;
        HANDLE *handlesList = new HANDLE[3];
        ZeroMemory(handlesList, 3 * sizeof(HANDLE));

        if ( si.hStdInput != INVALID_HANDLE_VALUE )
            handlesList[num++] = si.hStdInput;
        if ( si.hStdOutput != INVALID_HANDLE_VALUE )
            handlesList[num++] = si.hStdOutput;
        if ( si.hStdError != INVALID_HANDLE_VALUE )
            handlesList[num++] = si.hStdError;

        fSuccess = UpdateProcThreadAttribute(ssi.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handlesList, num * sizeof(HANDLE), NULL, NULL);
    }

    ZeroMemory(&ssi, sizeof(ssi));
    ssi.StartupInfo = si;
    ssi.StartupInfo.cb = sizeof(si);

    ZeroMemory( &psa, sizeof(psa) );
	psa.nLength = sizeof(psa);
	psa.bInheritHandle = true;

	ZeroMemory( &tsa, sizeof(tsa) );
	tsa.nLength = sizeof(tsa);
	tsa.bInheritHandle = true;

	if ( nomask )
	{
	    cmd = cmd_list.getString(' ');
	}
	else
	{
	    cmd = ToString::mascarade(cmd_list[0].c_str(), "\"");
	    for ( i =1; i<cmd_list.size(); i++)
	        cmd += " \"" + ToString::mascarade(cmd_list[i].c_str(), "\"") + "\"";
	}

	if( ! fSuccess || !CreateProcess( NULL,   // No module name (use command line).
			(CHAR *)cmd.c_str(),              // Command line.
			&psa,                             // Process handle inheritable.
			&tsa,                             // Thread handle inheritable.
			TRUE,                             // Handle inheritance
			EXTENDED_STARTUPINFO_PRESENT,     // Creation flags.
			NULL,                             // Use parent's environment block.
			NULL,                             // Use parent's starting directory.
			(STARTUPINFO*)(&ssi),             // Pointer to STARTUPINFO structure.
			&pi )                             // Pointer to PROCESS_INFORMATION structure.
	)
	{
	    msg.perror(E_START, "%d %s", GetLastError(), msg.getSystemerror(GetLastError()).c_str());
		msg.perror(E_START,"Kommando <%s> konnte nicht ausgeführt werden", cmd.c_str());
		SetEnvironmentVariable("PATH", path);

	    if ( have_logfile )
	    {
	        have_logfile = 0;
	        CloseHandle(si.hStdError);
	    }

	    if ( have_pipe )
	    {
	        have_pipe = 0;

	        CloseHandle(si.hStdOutput);
	        CloseHandle(si.hStdInput);
	        CloseHandle(pipew);
	        CloseHandle(piper);
	    }

        if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

        if (fInitialized) DeleteProcThreadAttributeList(ssi.lpAttributeList);
        if (ssi.lpAttributeList != NULL) HeapFree(GetProcessHeap(), 0, ssi.lpAttributeList);

		return 0;
	}

    if (fInitialized) DeleteProcThreadAttributeList(ssi.lpAttributeList);
    if (ssi.lpAttributeList != NULL) HeapFree(GetProcessHeap(), 0, ssi.lpAttributeList);

    if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
    {
        msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);
    }
	if ( extrapath != NULL && *extrapath != '\0' )
	    SetEnvironmentVariable("PATH", path);

	if ( have_pipe )
	{
		pthread_create(&waitid, NULL, ProcessWaitStop, (void *)this);
		waitidvalid = 1;
	}

	return 1;

#else
    int sockets[2];
    char cwd[PATH_MAX + 1];

    this->file = -1;
    this->cmd = cmd_list.getString(' ');

    if ( pipe )
    {
        if ( socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) )
        {
            msg.perror(E_PIPE, "kann socketpair nicht öffnen");
            return 0;
        }
    }

    pid = vfork();
	if ( pid < 0 )
	{
		msg.perror(E_FORK,"vfork konnte nicht durchgeführt werden");
		return 0;
	}
	if ( pid == 0 )
	{
		char **argv = new char*[cmd_list.size()+ 1];
		unsigned int i;
        int ifile;

		for ( i=0; i< cmd_list.size(); ++i)
			argv[i] = strdup(cmd_list[i].c_str());

		argv[i] = NULL;



        ifile = open("/dev/null", O_RDONLY );
        dup2(ifile,0);
        close(ifile);

        if ( pipe )
        {
            dup2(sockets[1],1);
            if ( logfile == "" )
                dup2(sockets[1],2);

            close(sockets[1]);
        }

        if ( logfile != ""  )
		{
			std::string lf;
			int lfile;

			if ( logdir != NULL && *logdir != '\0' )
				lf = (std::string)logdir + "/" + logfile;
			else
				lf = logfile;

			lfile = open(lf.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );
			if ( lfile < 0 )
			{
			    msg.perror(E_LOGFILE, "konnte logfile <%s> nicht öffnen", lf.c_str());
			    _exit(-2);
			}

			if ( ! pipe )
			    dup2(lfile,1);
			dup2(lfile,2);

			close(lfile);
		}

        if ( workdir != NULL && chdir(workdir) < 0 )
        {
             msg.perror(E_FOLDER, "kann nicht in Ordner <%s> wechseln", workdir);
            _exit(-1);
        }

        if ( workdir == NULL )
        {
            if ( ( workdir = getcwd(cwd, sizeof(cwd))) == NULL )
            {
                msg.perror(E_FOLDER, "kann aktuellen Ordner nicht ermitteln");
                _exit(-1);
            }

        }

        if ( extrapath == NULL )
            extrapath = "";

	    std::string path = getenv("PATH");
  	    setenv("PATH", (path + ":" + extrapath).c_str(), 1);

  	    if ( access((std::string(workdir) + "/" + argv[0]).c_str(), X_OK ) == 0)
  	    {
  	        execve((std::string(workdir) + "/" + argv[0]).c_str(), argv, environ);

  	         msg.perror(E_START,"Kommando <%s> konnte nicht ausgeführt werden",(std::string(workdir) + "/" + argv[0]).c_str());
  	        _exit(-3);
  	    }

  	    CsList pathlist( (path + ":" + extrapath), ':');
  	    for ( i=0; i<pathlist.size(); ++i )
  	    {
  	        if ( access((pathlist[i] + "/" + argv[0]).c_str(), X_OK ) == 0)
  	            break;
  	    }

		if ( i != pathlist.size() )
		{
		    execve((pathlist[i] + "/" + argv[0]).c_str(), argv, environ);
    		msg.perror(E_START,"Kommando <%s> konnte nicht ausgeführt werden",(pathlist[i] + "/" + argv[0]).c_str());
		}
		else
		{
		    msg.perror(E_START,"Kommando <%s> konnte nicht gestartet werden",argv[0]);
		}

		_exit(-3);
	}
	else
	{
	    if ( pipe )
	    {
	        this->file = sockets[0];
	        close(sockets[1]);
	    }
	    return 1;
	}
#endif
	return 0;
}

int Process::stop()
{
#if defined(__MINGW32__) || defined(__CYGWIN__)

    if ( pi.hProcess == INVALID_HANDLE_VALUE )
        return -1;

    TerminateProcess(pi.hProcess, 0 );
    return wait();

#else
    if ( pid == -1 )
        return -1;

    msg.pdebug(1, "stop");
    kill(pid,SIGINT);
    return wait();
#endif
}

int Process::wait()
{
   #if defined(__MINGW32__) || defined(__CYGWIN__)
    lock("wait");

	if ( pi.hProcess == INVALID_HANDLE_VALUE )
	{
        if ( waitidvalid )
        {
        	Pthread_join(waitid,NULL);
        	waitidvalid = 0;
        }
	    unlock("wait invalid");
	    return status;
	}

    DWORD exitcode;
	WaitForSingleObject( pi.hProcess, INFINITE );
	GetExitCodeProcess ( pi.hProcess, &exitcode);
	status = exitcode;

    if ( have_logfile )
	{
        have_logfile = 0;
	    CloseHandle(si.hStdOutput);
	}

    if ( have_pipe )
    {
        have_pipe = 0;

        CloseHandle(si.hStdOutput);
        CloseHandle(si.hStdInput);
        CloseHandle(pipew);
        CloseHandle(piper);
    }


    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    pi.hProcess = INVALID_HANDLE_VALUE;

	unlock("wait valid");

	return exitcode;

#else
    msg.pdebug(1, "wait");
    if ( pid == -1 ) return status;
    waitpid(pid, &status, 0);
    msg.pdebug(1, "waitready");
    pid = -1;
	status = WEXITSTATUS(status);
	return status;
#endif
}

int Process::write(const char *buffer, int size)
{
#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( have_pipe )
    {
    DWORD anzahl;
    if ( WriteFile(pipew, buffer, size, &anzahl, NULL) )
        return anzahl;
    else
        {
            if ( GetLastError() == ERROR_BROKEN_PIPE )
            {
                lock("wait"); // Wartethread zeit geben zu terminieren.
                unlock("wait");
            }
            return -1;
        }
    }
#else
        fd_set wr;
        FD_ZERO(&wr);
        FD_SET(file, &wr);
        int sel = select( file + 1, (fd_set*)0, &wr, (fd_set*)0, NULL);
        if ( sel > 0 )
             return ::write(file, buffer, size);
        else
        {
            close(file);
            file = -1;
            wait();
        }
#endif
    return 0;
    }

int Process::read( char *buffer, int size)
{
    int result = -1;
#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( have_pipe )
    {
        DWORD anzahl = 0;

        if ( ReadFile(piper, buffer, size, &anzahl, NULL) )
            return anzahl;
        else
        {
            if ( GetLastError() == ERROR_BROKEN_PIPE )
            {
                lock("wait"); // Wartethread zeit geben zu terminieren.
                unlock("wait");
            }
            return -1;
        }
    }
#else
    if ( file >= 0 )
    {
        fd_set rd;
        struct timeval t;

        t.tv_sec = 0;
        t.tv_usec = 100000;

        int sel = 0;
        while ( sel == 0 )
        {
            FD_ZERO(&rd);
            FD_SET(file, &rd);

            sel = select( file + 1, &rd, (fd_set*)0, (fd_set*)0, &t );

            if ( sel > 0 )
                result = ::read(file, buffer, size );
            else
            {
                if ( waitpid(pid, &status, WNOHANG) > 0 )
                {
                    status = WEXITSTATUS(status);
                    close(file);
                    pid = -1;
                    sel = -1;
                    file = -1;
                }
                result = 0;
            }
        }
    }

#endif
    return result;
}


