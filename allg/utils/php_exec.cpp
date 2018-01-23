#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <argument/argument.h>
#include <utils/cslist.h>

#include "php_exec.h"

PhpExec::PhpExec(std::string cmd, HttpHeader *h )
        :Process(NULL, 0)
{
    Argument a;
    int anzahl;
    char buffer[1024];
    std::string str, s;
    char *c, *ce;
    int header_ready;
    CsList header;
    HttpVars::Vars *vars;
    HttpVars::Vars::iterator vi;
    std::string::size_type cpos = 0;
    char filename[1024];
    int log;

    str = std::string(a["PhpPath"]) + " " + cmd;
    vars = h->vars.p_getVars();
    for ( vi=vars->begin(); vi != vars->end(); ++vi)
    {
        s = vi->first + "=" + vi->second ;
        for (cpos = 0; (cpos = s.find(' ', cpos)) != std::string::npos; ) s.replace(cpos, 1, "+");
        str += " " + s;
    }

    vars = h->vars.p_getExtraVars();
    for ( vi=vars->begin(); vi != vars->end(); ++vi)
    {
        s = vi->first + "=" + vi->second;
        for (cpos = 0; (cpos = s.find(' ', cpos)) != std::string::npos; ) s.replace(cpos, 1, "+");
        str += " " + s;
    }

#if defined(__MINGW32__) || defined(__CYGWIN__)
    *filename = '\0';
    if ( getenv ("TEMP") != NULL)
    {
        strncpy(filename, getenv("TEMP"), sizeof(filename) -1 );
        strncat(filename, "\\HttpXXXXXX", sizeof(filename) - strlen(filename) - 1);
    }
    _mktemp_s(filename, strlen(filename) + 1);
    filename[sizeof(filename) - 1] = '\0';
#else
    int fd;
    strcpy(filename, "/tmp/PhpExecXXXXXX");
    fd = mkstemp(filename);
    close(fd);
#endif

    start(str.c_str(), (std::string("pipe:") + filename).c_str() );

    header_ready = 0;
    str.clear();
    while( ( anzahl = read(buffer, sizeof(buffer))) != 0 )
    {
        if ( anzahl > 0 )
        {
            if ( header_ready == 0 )
            {
                c = buffer;
                ce = c + anzahl;
                while ( c != ce )
                {
                    if ( *c == '\n' )
                    {
                        if ( str != "" )
                        {
                            header.setString(str, ':' );
                            if ( header[0] == "Content-Type" || header[0] == "Content-type") h->content_type = header[1];
                            else if ( header[0] != "X-Powered-By") msg.perror(E_PHP, str.c_str());
                            str.clear();
                        }
                        else
                        {
                            header_ready = 1;
                            add_contentb(h , c + 1, anzahl - ( c - buffer ) - 1);
                            c = ce - 1;
                        }
                    }
                    else if ( *c != '\r')
                        str.push_back(*c);
                    c++;
                }
            }
            else
            {
                add_contentb(h , buffer, anzahl);
            }
        }
        else if ( anzahl < 0 && errno != EAGAIN )
            break;

    }

    if (  ( log = wait() ) != 0 )
    {
        h->content_type = "text/plain";
        switch ( log )
        {
        case 1:
        case 2:
        case 4:
            break;
        default:
            msg.perror(E_PHP, "Fehler <%d> im Script <%s> gefunden", log, cmd.c_str());
        }
    }

    FILE *flog = fopen(filename, "r" );
    if ( flog != NULL )
    {
        char *b;
        long size;
        fseek( flog, 0, SEEK_END);
        size = ftell(flog);
        fseek( flog, 0, SEEK_SET);

        if ( size < 0 ) size = 0;
        b = new char[size + 1];
        b[size] = '\0';
        fread(b, size, 1, flog);
        fclose(flog);

        switch ( log )
        {
        case 1:
            msg.perror(E_PHP, "%s", b);
            break;
        case 2:
            msg.pwarning(W_PHP, "%s", b);
            break;
        case 4:
            msg.pmessage(M_PHP, "%s", b);
            break;
        default:
            if ( size > 0 ) msg.ptext(b, strlen(b));
        }

        delete b;
    }

#if defined(__MINGW32__) || defined(__CYGWIN__)
    DeleteFile(filename);
#else
   unlink(filename);
#endif

}

PhpExec::~PhpExec()
{

}
