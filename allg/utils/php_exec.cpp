#define WINVER 0x0500
#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <argument/argument.h>
#include <utils/cslist.h>

#include "php_exec.h"

PhpExec::PhpExec(std::string cmd, HttpHeader *h)
        :Process(NULL)
{
    Argument a;
    int anzahl;
    char buffer[1024];
    std::string str;
    char *c, *ce;
    int header_ready;
    CsList header;
    HttpVars::Vars *vars;
    HttpVars::Vars::iterator vi;

    str = (char*)a["PhpPath"] + std::string(" ") + cmd;

    vars = h->vars.p_getVars();

    for ( vi=vars->begin(); vi != vars->end(); ++vi)
        str += " " + vi->first + "=" + vi->second;

    start(str.c_str(), "pipe" );

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
                            if ( header[0] == "Content-Type" || header[0] == "Content-type")
                                h->content_type = header[1];
                            str.clear();
                        }
                        else
                        {
                            header_ready = 1;
                            fwrite(c + 1, anzahl - ( c - buffer ) - 1, 1, h->content );
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
                fwrite(buffer, anzahl, 1, h->content );
            }
        }
        else if ( anzahl < 0 && errno != EAGAIN )
            break;

    }
    wait();
}

PhpExec::~PhpExec()
{

}
