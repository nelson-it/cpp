#ifndef process_mne
#define process_mne

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <windows.h>
#include <unistd.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include <ipc/timeout.h>
#include <message/message.h>
#include <utils/cslist.h>

class Process : public TimeoutClient
{
    std::string cmd;
    pid_t pid;
    int status;
    ServerSocket *s;

    #ifdef PTHREAD
    pthread_mutex_t mutex;
    #endif

    #if defined(__MINGW32__) || defined(__CYGWIN__)

    PROCESS_INFORMATION     pi;
    STARTUPINFO             si;
    int                     have_logfile;
    int                     have_pipe;
    HANDLE piper, pipew;
    pthread_t waitid;
    CRITICAL_SECTION    cs;
    #endif

protected:
    Message msg;

    enum ERROR_TYPES
    {
        E_FOLDER,
	E_PATH,
	E_START,
	E_LOGFILE,
	E_DEAD,
	E_FORK,
	E_PIPE,

	E_MAX = 100
    };
    int file;

public:
    Process(ServerSocket *s)
        : TimeoutClient(s),
	  msg("PROCESS")
	  {
            pid = -1;
#if defined(__MINGW32__) || defined(__CYGWIN__)
            have_pipe = 0;
            have_logfile = 0;
            InitializeCriticalSection(&cs);
#endif
            #ifdef PTHREAD
            pthread_mutex_init(&mutex,NULL);
            #endif
	  };

    ~Process()
    {
#if ! defined(__MINGW32__) && !! defined(__CYGWIN__)
        if ( file >=0 )
        {
            pthread_mutex_lock(&mutex);
            close(file);
            file = -1;
            pthread_mutex_unlock(&mutex);
        }
#endif
        stop();
    }

    int  start(const char* cmd, const char *logfile = NULL,
           const char *workdir = NULL, const char *logdir = NULL,
           const char *extrapath = NULL);

    int  start(CsList cmd, const char *logfile = NULL,
           const char *workdir = NULL, const char *logdir = NULL,
           const char *extrapath = NULL);

    void timeout( long sec, long usec, long w_sec, long w_usec);

    int write(const char *buffer, int size);
    int read( char *buffer, int size);

    int wait();
    int stop();

    int getPid() { return this->pid; }
    int getStatus()
    {
#if defined(__MINGW32__) || defined(__CYGWIN__)
        if ( have_pipe )
            pthread_join(waitid, NULL);
            pthread_detach(waitid);
#endif
        return this->status;
    }
};

#endif /* process_mne */

