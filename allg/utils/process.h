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

#if 1
#define Pthread_mutex_lock(x,y)     pthread_mutex_lock(y);
#define Pthread_mutex_unlock(x,y)   pthread_mutex_unlock(y);
#define Pthread_join(x, y)          pthread_join(x,y);
#else
#define Pthread_mutex_lock(x,y)  msg.pdebug(0,"lock %s %x", x, (unsigned long)y);\
        pthread_mutex_lock(y);
#define Pthread_mutex_unlock(x, y)  msg.pdebug(0,"unlock %s %x", x, (unsigned long)y);\
        pthread_mutex_unlock(y);
#define Pthread_join(x,y) msg.pdebug(0, "join to thread %x", (unsigned int)x); pthread_join(x,y);
#endif

class Process : public TimeoutClient
{
    std::string cmd;
    pid_t pid;
    int status;

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
    int waitidvalid;
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
  msg("PROCESS", 1)
{
        pid = -1;
#if defined(__MINGW32__) || defined(__CYGWIN__)
        have_pipe = 0;
        have_logfile = 0;
        waitidvalid = 0;
#endif
#ifdef PTHREAD
        pthread_mutex_init(&mutex,NULL);
#endif
};

    ~Process();

    int  start(const char* cmd, const char *logfile = NULL,
            const char *workdir = NULL, const char *logdir = NULL,
            const char *extrapath = NULL, int nomask = 0);

    int  start(CsList cmd, const char *logfile = NULL,
            const char *workdir = NULL, const char *logdir = NULL,
            const char *extrapath = NULL, int nomask = 0);

    void timeout( long sec, long usec, long w_sec, long w_usec);

    int write(const char *buffer, int size);
    int read( char *buffer, int size);

    int wait();
    int stop();

    void lock(const char* str)
    {
        Pthread_mutex_lock(str, &mutex);
    }

    void unlock(const char *str)
    {
        Pthread_mutex_unlock(str, &mutex);
    }

    int getPid() { return this->pid; }
    int getStatus()
    {
        return wait();
    }

};

#endif /* process_mne */

