#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#if ! defined(__MINGW32__) && ! defined(__CYGWIN__)
#include <sys/wait.h>
#include <sys/socket.h>
#else
#include <utils/tostring.h>
#endif

#if defined (MACOS)
extern char **environ;
#endif

#include <signal.h>
#include <fcntl.h>
#include <string.h>

#include <utils/cslist.h>

#include "process.h"

#if ! defined(__MINGW32__) && ! defined(__CYGWIN__)
static std::set<Process*> processes;

static void sig_child(int signum)
{
    std::set<Process*>::iterator i;
    for ( i = processes.begin(); i != processes.end(); ++i )
    {
        (*i)->timeout(0,0,0,0);
        if ( (*i)->getPid() == -1 )
        {
            processes.erase(i);
            return;
        }
    }

}
#else

void *ProcessWaitStop(void *param)
{
    Process *p = (Process*)param;
    p->wait();
    pthread_exit(NULL);
    return NULL;
}

#endif

int Process::start(const char *cmd, const char *logfile,
		const char *workdir, const char *logdir,
		const char *extrapath)
{
    CsList cmd_list(cmd, ' ');
    return start(cmd_list, logfile, workdir, logdir, extrapath);
}

int Process::start(CsList cmd_list, const char *logfile,
        const char *workdir, const char *logdir,
        const char *extrapath)
{
    this->status = -1;

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

    EnterCriticalSection(&cs);

	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	si.hStdInput = INVALID_HANDLE_VALUE;
	si.hStdOutput = INVALID_HANDLE_VALUE;
	si.hStdError = INVALID_HANDLE_VALUE;

    if ( logfile != NULL && logfile == std::string("pipe") )
    {
        std::string lf;

        //SECURITY_ATTRIBUTES sa;
        //sa.nLength = sizeof(sa);
        //sa.lpSecurityDescriptor = NULL;
        //sa.bInheritHandle = TRUE;

        if ( ! CreatePipe(&si.hStdInput, &pipew, NULL, 0) )
        {
            msg.perror(E_PIPE, "kann Pipe nicht erzeugen");
            LeaveCriticalSection(&cs);

            if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                    msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

            return 0;
        }

        if ( ! SetHandleInformation(si.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) )
        {
            msg.perror(E_PIPE, "kann Handleinformationen nicht setzen");
            LeaveCriticalSection(&cs);

            if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                    msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

            return 0;
        }

        if ( ! CreatePipe(&piper, &si.hStdOutput, NULL, 0) )
        {
            msg.perror(E_PIPE, "kann Pipe nicht erzeugen");
            LeaveCriticalSection(&cs);

            if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                    msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

            return 0;
        }

        if ( ! SetHandleInformation(si.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) )
        {
            msg.perror(E_PIPE, "kann Handleinformationen nicht setzen");
            LeaveCriticalSection(&cs);

            if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                    msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

            return 0;
        }

        si.hStdError  = si.hStdOutput;
        have_pipe = 1;
    }
    else if ( logfile != NULL && *logfile != '\0' )
    {
        std::string lf;

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        if ( logdir != NULL && *logfile != '\\' )
            lf = (std::string)logdir + "\\" + (std::string)logfile;
        else
            lf = logfile;

        si.hStdOutput = CreateFile(lf.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                &sa,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);

        si.hStdError  = si.hStdOutput;
        if ( si.hStdOutput == INVALID_HANDLE_VALUE )
        {
            msg.perror(E_LOGFILE, "konnte logfile <%s> nicht öffnen", logfile);
            SetEnvironmentVariable("PATH", path);
            LeaveCriticalSection(&cs);

            if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                    msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

            return 0;
        }
        have_logfile = 1;
    }
    else
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

	cmd = ToString::mascarade(cmd_list[0].c_str(), "\"");
	for ( i =1; i<cmd_list.size(); i++)
	    cmd += " \"" + ToString::mascarade(cmd_list[i].c_str(), "\"") + "\"";

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

        if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
                msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

        if (fInitialized) DeleteProcThreadAttributeList(ssi.lpAttributeList);
        if (ssi.lpAttributeList != NULL) HeapFree(GetProcessHeap(), 0, ssi.lpAttributeList);

       LeaveCriticalSection(&cs);
		return 0;
	}

    if (fInitialized) DeleteProcThreadAttributeList(ssi.lpAttributeList);
    if (ssi.lpAttributeList != NULL) HeapFree(GetProcessHeap(), 0, ssi.lpAttributeList);

    if ( workdir != NULL && *workdir != '\0' && SetCurrentDirectory(actdir) == 0 )
        msg.pwarning(E_FOLDER, "kann nicht in Ordner <%s> wechseln", actdir);

	if ( extrapath != NULL && *extrapath != '\0' )
	    SetEnvironmentVariable("PATH", path);

	if ( have_pipe )
	    pthread_create(&waitid, NULL, ProcessWaitStop, (void *)this);

    LeaveCriticalSection(&cs);
	return 1;

#else
    int sockets[2];
    int pipe = 0;

    this->file = -1;
    this->cmd = cmd_list.getString(' ');

    if ( logfile != NULL && logfile == std::string("pipe") )
    {
        pipe = 1;
        if ( socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) )
        {
            msg.perror(E_PIPE, "kann socketpair nicht öffnen");
            return 0;
        }
    }

    signal(SIGCHLD, sig_child);
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

		if ( workdir != NULL && chdir(workdir) < 0 )
		{
			 msg.perror(E_FOLDER, "kann nicht in Ordner <%s> wechseln", workdir);
			_exit(-1);
		}

        ifile = open("/dev/null", O_RDONLY );
        dup2(ifile,0);
        close(ifile);

        if ( pipe )
        {
            dup2(sockets[1],1);
            dup2(sockets[1],2);

            close(sockets[1]);
            //close(0);
        }
        else if ( logfile != NULL && *logfile != '\0' )
		{
			std::string lf;
			int lfile;

			if ( logdir != NULL && *logdir != '\0' )
				lf = (std::string)logdir + "/" + (std::string)logfile;
			else
				lf = logfile;

			lfile = open(lf.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );
			if ( lfile < 0 )
			{
			    msg.perror(E_LOGFILE, "konnte logfile <%s> nicht öffnen", logfile);
			    _exit(-2);
			}

			dup2(lfile,1);
			dup2(lfile,2);

			close(lfile);
			//close(0);
		}

        if ( extrapath == NULL )
            extrapath = "";

	    std::string path = getenv("PATH");
  	    setenv("PATH", (path + ":" + extrapath).c_str(), 1);

  	    if ( access(argv[0], X_OK ) == 0)
  	    {
  	        execve(argv[0], argv, environ);

  	         msg.perror(E_START,"Kommando <%s> konnte nicht ausgeführt werden",argv[0]);
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
    	    msg.perror(E_START,"Kommando <%s> konnte nicht gestartet werden",argv[0]);

		_exit(-3);
	}
	else
	{
        processes.insert(this);
	    if ( pipe )
	    {
	        this->file = sockets[0];
	    }
	    setInterval(10);
	    return 1;
	}
#endif
	return 0;
}

void Process::timeout(long sec, long usec, long w_sec, long w_usec)
{

#if defined(__MINGW32__) || defined(__CYGWIN__)

	DWORD result;

	if ( ( result = WaitForSingleObject( pi.hProcess, 0 )) == WAIT_OBJECT_0 )
	{
	    msg.perror(E_DEAD, "Prozess <%s> wurde abgebrochen", cmd.c_str());
		setInterval(0);
	}
#else
    pthread_mutex_lock(&mutex);
    int result = 0;
    if ( pid != -1 )
    {
        result = waitpid(pid, &status, WNOHANG);
        status = WEXITSTATUS(status);
    }

    if ( result != 0 )
    {
        setInterval(0);
        pid = -1;
        if ( file != 0 )
        {
           close(file);
           file = -1;
        }
    }
    pthread_mutex_unlock(&mutex);

#endif
}

int Process::stop()
{
#if defined(__MINGW32__) || defined(__CYGWIN__)

    if ( pi.hProcess == INVALID_HANDLE_VALUE )
        return -1;

    setInterval(0);

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
   setInterval(0);
   #if defined(__MINGW32__) || defined(__CYGWIN__)

   pthread_mutex_lock(&mutex);
   DWORD exitcode;

	if ( pi.hProcess == INVALID_HANDLE_VALUE )
		return -1;

	WaitForSingleObject( pi.hProcess, INFINITE );
	GetExitCodeProcess ( pi.hProcess, &exitcode);
	status = exitcode;

	// Close process and thread handles.
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

    pi.hProcess = INVALID_HANDLE_VALUE;

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

	pthread_mutex_unlock(&mutex);

	return exitcode;

#else
    msg.pdebug(1, "wait");
    if ( pid == -1 ) return -1;
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
        return -1;
    }
#else
    if ( file >= 0 ) return ::write(file, buffer, size);
#endif
    return 0;
    }

int Process::read( char *buffer, int size)
{
#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( have_pipe )
    {
        DWORD anzahl;
        if ( ReadFile(piper, buffer, size, &anzahl, NULL) )
            return anzahl;
        else
            return -1;
    }
#else
    if ( file >= 0 ) return ::read(file, buffer, size);
#endif
    return 0;
}


