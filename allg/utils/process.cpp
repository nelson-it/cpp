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
    this->cmd = cmd_list.getString(' ');
    this->status = -1;

    #if defined(__MINGW32__) || defined(__CYGWIN__)

	SECURITY_ATTRIBUTES psa;
	SECURITY_ATTRIBUTES tsa;
	char path[1024];

	ZeroMemory( &si, sizeof(si) );
	ZeroMemory( &pi, sizeof(pi) );
	pi.hProcess = INVALID_HANDLE_VALUE;
	have_logfile = 0;

	if ( workdir != NULL && *workdir != '\0' )
	{
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
		SetEnvironmentVariable("PATH",
				((std::string)path + ";" +
						(std::string)extrapath).c_str());
	}

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	// si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

	if ( logfile != NULL && *logfile != '\0' )
	{
		std::string lf;

		SECURITY_ATTRIBUTES sa;
		sa.nLength = sizeof(sa);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = TRUE;

		if ( logdir != NULL && *logdir != '\0' )
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
			return 0;
		}
		have_logfile = 1;
	}
	else
	{
		si.hStdOutput  = GetStdHandle(STD_ERROR_HANDLE);
		si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
		have_logfile = 0;
	}


	ZeroMemory( &psa, sizeof(psa) );
	psa.nLength = sizeof(psa);
	psa.bInheritHandle = true;

	ZeroMemory( &tsa, sizeof(tsa) );
	tsa.nLength = sizeof(tsa);
	tsa.bInheritHandle = true;

	if( !CreateProcess( NULL, // No module name (use command line).
			(CHAR *)cmd.c_str(),          // Command line.
			&psa,                 // Process handle inheritable.
			&tsa,                 // Thread handle inheritable.
			TRUE,                 // Set handle inheritance to FALSE.
			0,                    // No creation flags.
			NULL,                 // Use parent's environment block.
			NULL,                 // Use parent's starting directory.
			&si,          // Pointer to STARTUPINFO structure.
			&pi )         // Pointer to PROCESS_INFORMATION structure.
	)
	{
		msg.perror(E_START,"Kommando <%s> konnte nicht ausgeführt werden", cmd.c_str());
		SetEnvironmentVariable("PATH", path);
		return 0;
	}

	if ( extrapath != NULL && *extrapath != '\0' )
	    SetEnvironmentVariable("PATH", path);

	setInterval(10);
	return 1;

#else
    int sockets[2];
    int pipe = 0;
    this->file = -1;

    if ( logfile != NULL && logfile == std::string("pipe") )
    {
        pipe = 1;
        if ( socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) )
        {
            msg.perror(E_PIPE, "kann socketpair nicht öffnen");
            return 0;
        }
        processes.insert(this);
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

		for ( i=0; i< cmd_list.size(); ++i)
			argv[i] = strdup(cmd_list[i].c_str());

		argv[i] = NULL;

		if ( workdir != NULL && chdir(workdir) < 0 )
		{
			msg.perror(E_FOLDER, "kann nicht in Ordner <%s> wechseln", workdir);
			exit(-1);
		}

        if ( pipe )
        {
            dup2(sockets[1],1);
            dup2(sockets[1],2);

            close(sockets[1]);
            close(0);
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
				exit(-1);
			}

			dup2(lfile,1);
			dup2(lfile,2);

			close(lfile);
			close(0);
		}

		if ( extrapath != NULL && *extrapath != '\0' )
		{
			std::string path = getenv("PATH");
			setenv("PATH", (path + ":" + extrapath).c_str(), 1);
		}

		execve(argv[0], argv, environ);
		msg.perror(E_START,"Kommando <%s> konnte nicht ausgeführt werden",cmd.c_str());

		_exit(-1);
	}
	else
	{
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
	int result = waitpid(pid, &status, WNOHANG);

	if ( result == 0 )
		return;

	setInterval(0);

	if ( file != 0 )
	{
        pthread_mutex_lock(&mutex);
	    close(file);
	    file = -1;
        pthread_mutex_unlock(&mutex);
	}

	pid = -1;

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

	DWORD exitcode;

	if ( pi.hProcess == INVALID_HANDLE_VALUE )
		return -1;

	WaitForSingleObject( pi.hProcess, INFINITE );
	GetExitCodeProcess ( pi.hProcess, &exitcode);

	if ( have_logfile )
		CloseHandle(si.hStdOutput);

	// Close process and thread handles.
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	have_logfile = 0;
	pi.hProcess = INVALID_HANDLE_VALUE;

	status = exitcode;
	return exitcode;

#else
	msg.pdebug(1, "wait");
	if ( pid == -1 ) return -1;
	waitpid(pid, &status, 0);
	msg.pdebug(1, "waitready");
	pid = -1;
	return status;
#endif
}

