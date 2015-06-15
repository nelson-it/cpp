#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <argument/argument.h>
#include <utils/process.h>

#include "dbconnect.h"
#include "dbhttputils_trust.h"

DbHttpUtilsTrust::DbHttpUtilsTrust(DbHttp *h)
:DbHttpProvider(h),
 msg("DbHttpUtilsTrust")
{
    subprovider["checkuser.html"] = &DbHttpUtilsTrust::check_user;
    h->add_provider(this);
}

DbHttpUtilsTrust::~DbHttpUtilsTrust()
{
}

int DbHttpUtilsTrust::check_request(Database *db, HttpHeader *h)
{
    return 1;
}

int DbHttpUtilsTrust::request(Database *db, HttpHeader *h, int nologin )
{
    std::string name;
    std::string::size_type n;

    SubProviderMap::iterator i;

    if ( (i = subprovider.find(h->filename)) != subprovider.end() )
    {
        h->status = 200;
        (this->*(i->second))(db, h);

        if (h->vars["sqlend"] != "")
            db->p_getConnect()->end();

        return 1;
    }

    n = h->filename.find_last_of('.');
    if ( n != std::string::npos )

        name = h->filename.substr(0, n);

    h->status = 200;
    this->execute(db, h, name, nologin );
    return 1;

}

int DbHttpUtilsTrust::request(Database *db, HttpHeader *h)
{
    return request(db, h, 0);
}

void DbHttpUtilsTrust::check_user(Database *db, HttpHeader *h)
{
    Argument a;
    Database *d = db->getDatabase();
    unsigned int i;
    int host;

    Argument::StringWerte ips;
    ips = (a["DbTrustCheckUserip"]).getStringWerte();

    host = this->http->getServersocket()->getHost(h->client);
    for ( i = 0; i < ips.size(); ++i )
        if (  check_ip(ips[i].c_str(), host ) ) break;

    d->p_getConnect("", h->user, h->passwd);

    if ( i != ips.size() && d->have_connection() )
        h->status = 200;
    else
        h->status = 401;

    delete d;

}
void DbHttpUtilsTrust::execute(Database *db, HttpHeader *h, std::string name, int nologin )
{
    std::string stm;
    char buffer[1024];

    CsList cols;
    DbTable::ValueMap where;
    DbConnect::ResultMat* result;
    DbConnect::ResultMat::iterator ri;
    int host;

    DbTable *tab = db->p_getTable(db->getApplschema(), "trustrequest");

    cols.add("action");
    cols.add("ipaddr");
    cols.add("typ");
    cols.add("validpar");

    where["name"] = name;

    result = tab->select(&cols, &where);
    tab->end();

    if ( nologin )
    {
        host = this->http->getServersocket()->getHost(h->client);
        for ( ri = result->begin(); ri != result->end(); ++ri )
            if (  check_ip(((std::string) (*ri)[1]).c_str(), host ) ) break;
    }
    else
    {
        ri = result->begin();
    }

    if ( ri == result->end() )
    {
        h->content_type = "text/plain";
        h->status = 404;
        rewind(h->content);
        fprintf(h->content, "error");
        msg.perror(E_NOFUNC, "keine Funktion für den Namen <%s> gefunden", name.c_str());
        return;
    }

    if ( (std::string) (*ri)[2] == "sql" )
    {
        stm = (std::string) (*ri)[0];

        if ( db->p_getConnect()->execute(stm.c_str()) == 0 )
        {
            DbConnect::ResultMat *r = db->p_getConnect()->p_getResult();
            if ( r->size() == 0 )
                fprintf(h->content,"ok");
            else
                fprintf(h->content,"%s",ToString::mkhtml(((*r)[0][0]).format(&msg)).c_str());
        }
        else
        {
            fprintf(h->content,"error");
        }

        db->p_getConnect()->end();
    }
    else if ( (std::string) (*ri)[2] == "shell" )
    {
        Process p(DbHttpProvider::http->getServersocket());
        CsList cmd;
        CsList validpar((char *)((*ri)[3]));
        int anzahl;
        HttpVars::Vars::iterator i;
        std::map<std::string,std::string>::iterator m;
        Argument a;

        cmd.add((std::string) (*ri)[0]);

        DbHttpAnalyse::Client::Userprefs userprefs = this->http->getUserprefs();
        DbHttpAnalyse::Client::Userprefs::iterator ui;
        for ( ui = userprefs.begin(); ui != userprefs.end(); ++ui)
        {
            cmd.add("-" + ui->first);
            cmd.add(ui->second);
        }

        for (m = h->datapath.begin(); m != h->datapath.end(); ++m )
        {
            cmd.add("-datapath");
            cmd.add(m->first);
            cmd.add(ToString::substitute(ToString::substitute(h->dataroot + m->second.c_str(), "\\", "/"), "C:", "/cygdrive/c"));
        }

        cmd.add("-db");
        cmd.add(a["DbDatabase"]);

        cmd.add("-content_type");
        cmd.add(h->content_type);

        cmd.add("-hostname");
        cmd.add(h->hostname);

        cmd.add("-port");
        cmd.add(h->port);

        for ( i= h->vars.p_getVars()->begin(); i != h->vars.p_getVars()->end(); ++i )
        {
            if ( validpar.empty() || validpar.find(i->first) != std::string::npos )
            {
                cmd.add("-" + i->first);
                cmd.add(i->second);
            }
            else
            {
                h->content_type = "text/plain";
                h->status = 404;
                rewind(h->content);
                fprintf(h->content, "error");
                msg.perror(E_NOFUNC, "keine Funktion für den Namen <%s> gefunden", name.c_str());
                return;
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
        p.start(cmd, "pipe", a["projectroot"], NULL, NULL, 1);

        while( ( anzahl = p.read(buffer, sizeof(buffer))) != 0 )
        {
            if ( anzahl > 0 )
                fwrite(buffer, 1, anzahl, h->content);
            else if ( anzahl < 0 && errno != EAGAIN ) break;
        }

        if ( p.getStatus() != 0 )
        {
            h->content_type = "text/plain";
            snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", "error.txt");
            buffer[sizeof(buffer) -1] = '\0';
            h->extra_header.push_back(buffer);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", h->vars.url_decode(h->filename.c_str()).c_str());
            buffer[sizeof(buffer) -1] = '\0';
            h->extra_header.push_back(buffer);
        }
    }
    else
    {
        h->content_type = "text/plain";
        snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", "error.txt");
        buffer[sizeof(buffer) -1] = '\0';
        h->extra_header.push_back(buffer);

        rewind(h->content);
        fprintf(h->content, "error");
        msg.perror(E_NOFUNC, "keine Funktion für den Namen <%s> gefunden", name.c_str());
        return;

    }
}

