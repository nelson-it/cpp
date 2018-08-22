#include <pthread.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)

#else

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#if defined (Darwin) || defined(LINUX)
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
    int daemon;
    std::string logfile, runningdir, lockfile;
    mode_t mask;

    deamon = 0;

    runningdir = (std::string)a["rundir"];
    daemon     = (long)a["daemon"];
    mask       = (long)a["umask"];

    if ( runningdir != "" )
    {
        if ( chdir(runningdir.c_str()) != 0 )
            msg.perror(1, "kann nicht in Verzeichnis <%s> wechseln", runningdir.c_str() );
    }

    if ( mask != 0 )
        umask(mask);

    if ( geteuid() == 0 )
    {
        struct passwd *pwd = NULL;
        struct group *grp = NULL;
        std::string runuser, rungroup;

        runuser = (std::string)a["runuser"];
        if ( runuser != "" )
        {
            pwd = getpwnam(runuser.c_str());
            if ( pwd == NULL )
            {
                msg.perror(1, "Benutzer <%s> ist unbekannt", ((std::string)a["runuser"]).c_str() );
                exit(1);
            }
        }

        rungroup = (std::string)a["rungroup"];
        if ( rungroup != "" )
        {
            grp = getgrnam(rungroup.c_str());
            if ( grp == NULL )
            {
                msg.perror(1, "Gruppe <%s> ist unbekannt", ((std::string)a["rungroup"]).c_str() );
                exit(1);
            }
        }

        if ( grp != NULL )
        {
            setgroups(0, NULL);
            setgid(grp->gr_gid);
        }
        else if ( pwd != NULL )
        {
            setgroups(0, NULL);
            setgid(pwd->pw_gid);
        }

        if ( pwd != NULL )
            setuid(pwd->pw_uid);

    }

    if (getppid() == 1) return; /* already a daemon */

    if ( daemon == 0 )
        return;

    deamon = 1;

    logfile = (std::string)a["logfile"];
    lockfile = (std::string)a["lockfile"];

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
    if ( logfile != "-")
        msg.mklog(logfile);

}
#endif
