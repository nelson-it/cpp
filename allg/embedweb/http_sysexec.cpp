#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <argument/argument.h>
#include <utils/process.h>
#include <utils/tostring.h>
#include <utils/tmpfile.h>

#include "http_sysexec.h"

HttpSysexec::HttpSysexec(Http *h) :
HttpProvider(h), msg("HttpSysexec")
{
    Argument a;
    rexec = (std::string)a["HttpSysexecCommand"];

    if ( h != NULL )
        h->add_provider(this);
}

HttpSysexec::~HttpSysexec()
{
}

int HttpSysexec::request(HttpHeader *h)
{

    std::string str;
    std::string::size_type j;

    h->status = 200;
    h->translate = 1;
    h->content_type = "text/json";

    if ( h->filename != "index.html" )
    {
        if ( ( j = h->filename.find_last_of(".") ) != std::string::npos )
        {
            h->content_type = "application/octet-stream";
            h->translate = 0;
        }
    }

    execute(h);
    return 1;
}

void HttpSysexec::execute ( HttpHeader *h)
{
    Argument a;
    CsList cmd;
    std::string command;
    char buffer[1024];
    char *result;
    int anzahl, ranzahl, maxanzahl;
    unsigned int i;
    HttpVars::Vars::iterator vi;

    Argument::StringWerte ips;
    ips = (a["HttpSysexecUserip"]).getStringWerte();

    command = h->dirname;

    for ( i = 0; i < ips.size(); ++i )
        if (  this->http->check_ip(ips[i].c_str()) ) break;

    if ( i == ips.size() || ( h->user != "admindb" && this->http->check_group(h, "adminsystem") == 0 && this->http->check_sysaccess(h) == 0 ))
    {
        msg.perror(E_NOFUNC, "keine Berechtigung für %s", h->client_host.c_str() );
        if ( h->content_type == "text/json" )
            add_content(h,  "{ \"result\" : \"error\"\n" );
        else
            add_content(h,  "error");
        return;
    }

    cmd.add(rexec.c_str());
    if ( command.find_first_not_of("1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_/") != std::string::npos )
    {
        h->status = 404;
        del_content(h);
        msg.perror(E_NOFUNC, "keine Funktion für den Namen <%s> gefunden", command.c_str());
        return;
    }

    cmd.add("-locale");
    cmd.add(a["locale"]);

    cmd.add("-r");
    cmd.add(a["projectroot"]);

    cmd.add("-project");
    cmd.add(a["project"]);

    cmd.add("-path");
    cmd.add(a["HttpSysexecPath"]);

    cmd.add(command);

    if ( h->content_type != "text/json" )
    {
        cmd.add("filename");
        cmd.add(h->filename);
    }

    for ( vi = h->vars.p_getVars()->begin(); vi != h->vars.p_getVars()->end(); ++vi )
    {
        cmd.add(vi->first);
        cmd.add(vi->second);
    }

    TmpFile tmpfile("HttpSysexecXXXXXX");
    Process p;
    p.start(cmd, (std::string("pipe:") + std::string(tmpfile.get_name())).c_str(), std::string(a["projectroot"]).c_str(), NULL, ".", 1);

    Message log("HttpSysexec Kommando", 1);
    result = new char[10240];
    ranzahl = 0;
    maxanzahl = 10240;
    while( ( anzahl = p.read(buffer, sizeof(buffer))) != 0 )
    {
        if ( ranzahl + anzahl >= maxanzahl )
        {
            while ((ranzahl + anzahl) >= maxanzahl )
                maxanzahl += 10240;
            char *str = new char[maxanzahl];
            memcpy(str, result, ranzahl);
            delete[] result;
            result = str;
        }

        if ( anzahl > 0 )
        {
            memcpy(&result[ranzahl], buffer, anzahl);
            ranzahl += anzahl;
        }
        else if ( anzahl < 0 && errno != EAGAIN ) break;
    }

    if ( p.getStatus() != 0 )
    {
        if ( h->content_type == "text/json" )
        {
            std::string errstr = tmpfile.get_content();
            result[ranzahl] = '\0';

            msg.perror(E_ERRORFOUND, "%s %d", (( errstr.length() == 0) ? "#mne_lang#Fehler:#" : errstr.c_str()), p.getStatus());
            add_content(h,  "{ \"result\" : \"error:\"\n" );

            h->translate = 1;
        }
        else
        {
            add_content(h,  "error");
            add_content(h, tmpfile.get_content().c_str());
            add_contentb(h, result, ranzahl);
        }
    }
    else
    {
        if ( h->content_type == "text/json" )
        {
            if ( ranzahl )
            {
                h->translate = 1;
                add_contentb(h, result, ranzahl);
            }
            else
                add_content(h,  "{ \"result\" : \"ok\"\n" );

            std::string str = tmpfile.get_content();
            if ( str != "" )
                msg.pwarning(W_WARNINGFOUND, "%s", str.c_str());
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", h->filename.c_str());
            buffer[sizeof(buffer) -1] = '\0';
            h->extra_header.push_back(buffer);
            add_contentb(h, result, ranzahl );

        }
    }
}

