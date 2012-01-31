#ifndef process_mne
#define process_mne

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include <ipc/timeout.h>
#include <message/message.h>

class Process : public TimeoutClient
{
    Message msg;
    std::string cmd;
    pid_t pid;

    #if defined(__MINGW32__) || defined(__CYGWIN__)

    PROCESS_INFORMATION     pi;
    STARTUPINFO             si;
    int                     have_logfile;

    #endif

protected:
    enum ERROR_TYPES
    {
        E_FOLDER,
	E_PATH,
	E_START,
	E_LOGFILE,
	E_DEAD,
	E_FORK,
	E_PIPE
    };
    int file;

public:
    Process(ServerSocket *s)
        : TimeoutClient(s),
	  msg("PROCESS")
	  {
            pid = -1;
	  };

    ~Process()
    {
        if ( file >=0 ) close(file);
        stop();
    }
    int  start(const char* cmd, const char *logfile = NULL,
	       const char *workdir = NULL, const char *logdir = NULL,
	       const char *extrapath = NULL);

    void timeout( long sec, long usec, long w_sec, long w_usec);

    int write(const char *buffer, int size)
    {
        return ::write(file, buffer, size);
    }

    int read( char *buffer, int size)
    {
        return ::read(file, buffer, size);
    }

    int wait();
    int stop();

    int getPid() { return this->pid; }
};

#endif /* process_mne */

