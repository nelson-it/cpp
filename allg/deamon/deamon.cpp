#ifdef PTHREAD
#include <pthread.h>
#endif

#if defined(__MINGW32__) || defined(__CYGWIN__)

#else

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#ifdef Darwin
#include <sys/stat.h>
#endif

#include <argument/argument.h>
#include "deamon.h"

void signal_handler(int sig)
{
    Message msg("Signal");
        switch(sig) {
        case SIGCHLD:
            msg.pdebug(1,"child signal empfangen");
            break;
        case SIGHUP:
                msg.perror(1,"hangup signal empfangen");
                break;
        case SIGTERM:
            msg.perror(2,"terminate signal emfangen");
                exit(0);
                break;
        }
}


Deamon::Deamon()
       :msg("Deamon")
{
    Argument a;
    int i, lfp;
    char str[10];
    std::string logfile, runningdir, lockfile;

    if (getppid() == 1) return; /* already a daemon */

    deamon = 0;
    if ( (long)a["deamon"] == 0 )
        return;

    deamon = 1;

    logfile = (char *)a["logfile"];
    runningdir = (char*)a["rundir"];
    lockfile = (char*)a["lockfile"];

    i = fork();
    if (i < 0) exit(1); /* fork error */
    if (i > 0) exit(0); /* parent exits */

    /* child  */
    /* neue process grupppe */
    setsid();

    // descriptoren schliessen
    for (i = getdtablesize(); i >= 0; --i)
        close(i);

    /* handle standart I/O */
    i = open("/dev/null", O_RDWR);
    dup(i);
    dup(i);

    /* neue file permissions */
    umask(027);

    /* running directory setzen */
    if ( runningdir != "" )
        chdir(runningdir.c_str());

    /* lockfile überprüfen */
    if ( lockfile != "" )
    {
        if ( ( lfp = open(lockfile.c_str(), O_RDWR | O_CREAT, 0640)) < 0 )
            exit(1);

        if (lockf(lfp, F_TLOCK, 0) < 0)
            exit(0);

        /* pid ins lockfile */
        sprintf(str, "%d\n", getpid());
        write(lfp, str, strlen(str));
    }

    // signale abfangen
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, signal_handler);
    signal(SIGCHLD, signal_handler);
    signal(SIGTERM, signal_handler);

    // logfile in Message erzeugen
    msg.mklog(logfile);

}
#endif
