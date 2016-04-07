#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <argument/argument.h>
#include <utils/process.h>
#include <utils/tostring.h>

#include "http_sysexec.h"

HttpSysexec::HttpSysexec(Http *h) :
HttpProvider(h), msg("HttpSysexec")
{
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

    if ( h->filename != "index.html" )
    {
        if ( ( j = h->filename.find_last_of(".") ) != std::string::npos )
        {
            h->dirname = h->dirname + "/" + h->filename.substr(0, j);
            h->filename = h->filename.substr(j);
        }
    }
    else
    {
        h->content_type = "text/xml";
    }

    h->status = 200;
    execute(h);
    return 1;
}

void HttpSysexec::execute ( HttpHeader *h)
{
    Argument a;
    CsList cmd;
    std::string command;
    char buffer[1024];
    int anzahl;
    unsigned int i;
    int host;
    std::string logtext;
    HttpVars::Vars::iterator vi;

    Argument::StringWerte ips;
    ips = (a["HttpSysexecUserip"]).getStringWerte();

    host = this->http->getServersocket()->getHost(h->client);
    for ( i = 0; i < ips.size(); ++i )
        if (  check_ip(ips[i].c_str(), host ) ) break;

    if ( i == ips.size() || ( h->user != "admindb" && this->http->check_group(h, "adminsystem") == 0 ) )
    {
        msg.perror(E_NOFUNC, "keine Berechtigung");
        if ( h->content_type == "text/xml" )
            fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        else
            fprintf(h->content, "error");
        return;
    }

    cmd.add("exec/system/programs/rexec");
    command = h->dirname;
    if ( command.find_first_not_of("1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_/") != std::string::npos )
    {
        h->status = 404;
        rewind(h->content);
        msg.perror(E_NOFUNC, "keine Funktion für den Namen <%s> gefunden", command.c_str());
        return;
    }

    cmd.add("-locale");
    cmd.add(a["locale"]);

    cmd.add("-r");
    cmd.add(a["projectroot"]);

    cmd.add("-project");
    cmd.add(a["project"]);

    cmd.add(command);

    for ( vi = h->vars.p_getVars()->begin(); vi != h->vars.p_getVars()->end(); ++vi )
    {
        if ( vi->second != "" )
        {
            cmd.add(vi->first);
            cmd.add(vi->second);
        }
    }

#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::string str = "bash -c '";
    unsigned int j;
    for ( j = 0; j<cmd.size(); j++)
        str += " \"" + ToString::mascarade(cmd[j].c_str(), "\"") + "\"";
    str += "'";
    cmd.clear();
    cmd.add(str);
#endif
    Process p(http->getServersocket());
    p.start(cmd, "pipe", std::string(a["projectroot"]).c_str(), NULL, ".", 1);

    Message log("HttpSysexec Kommando", 1);
    int havelog = 0;
    logtext = command + "\n";
    while( ( anzahl = p.read(buffer, sizeof(buffer))) != 0 )
    {
        if ( anzahl > 0 )
        {
            havelog = 1;
            buffer[anzahl] = '\0';
            logtext = logtext + buffer;
        }
        else if ( anzahl < 0 && errno != EAGAIN ) break;
    }

    if ( p.getStatus() != 0 || havelog )
    {
        if ( p.getStatus() == 0 )
        {
            msg.pwarning(E_ERRORFOUND, logtext.c_str());
        }
        else
        {
            msg.perror(E_ERRORFOUND, "Fehler gefunden");
            msg.perror(E_ERRORFOUND, "%s", logtext.c_str());
        }

        if ( h->content_type == "text/xml" )
            fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        else
            fprintf(h->content, "error");
    }
    else
    {
        if ( h->content_type == "text/xml" )
            fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>ok</body>", h->charset.c_str());
        else
            fprintf(h->content, "ok");
    }
}

