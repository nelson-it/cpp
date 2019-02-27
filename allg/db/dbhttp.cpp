#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include <argument/argument.h>
#include <embedweb/http_provider.h>

#include "dbconnect.h"
#include "dbtranslate.h"
#include "dbtable.h"

#include "dbhttp.h"
#include "dbhttp_provider.h"
#include "dbhttp_application.h"


DbHttp::DbHttp(ServerSocket *s, DbHttpAnalyse *analyse, Database *db, int register_it) :
        Http(s, NULL ), msg("DbHttp")
{
    Argument a;
    char str[1024];

    this->analyse = analyse;
    this->act_client = NULL;
    this->trans = new DbTranslate(db);
#if defined(__MINGW32__) || defined(__CYGWIN__)
#else
    this->loc[a["locale"]] = this->stdloc = newlocale(LC_ALL_MASK, std::string(a["locale"]).c_str(), NULL);
#endif
    snprintf(str, sizeof(str), "MneHttpSessionId%d", (int)a["port"]);
    str[sizeof(str) - 1] = '\0';
    this->cookieid = str;

    application = new DbHttpApplication(this, db);
    if ( register_it)
        analyse->add_http(this);
}

DbHttp::~DbHttp()
{
    delete application;
    delete trans;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    // nichts tun
#else
    std::map<std::string, locale_t>::iterator i;

    for ( i = this->loc.begin(); i != this->loc.end(); ++i )
        freelocale(i->second);
#endif
}

void DbHttp::init_thread()
{
    msg.setMsgtranslator(this->trans);
}

void DbHttp::setLocale()
{
    Argument a;
    std::string lstr;
#if defined(__MINGW32__) || defined(__CYGWIN__)
    setlocale(LC_ALL,( act_client == NULL ) ? ((std::string)a["locale"]).c_str() :  act_client->getUserprefs("mslanguage").c_str());
#else
    std::map<std::string, locale_t>::iterator l;
    lstr = ( act_client == NULL ) ?  a["locale"] : act_client->getUserprefs("language") + "_" + act_client->getUserprefs("region") + ".UTF-8";
    if ( ( l = this->loc.find(lstr)) == this->loc.end() )
    {
        this->loc[lstr] = newlocale(LC_ALL_MASK, lstr.c_str(), NULL);
        l = this->loc.find(lstr);
    }
    uselocale(this->stdloc);
#endif
}

void DbHttp::make_content(HttpHeader *h)
{
    if ( h != NULL ) act_h = h;
    if ( act_h == NULL ) return;

    make_answer();
    make_error();
    make_translate();
}

void DbHttp::make_answer()
{
    act_client = analyse->getClient(act_h->cookies[cookieid.c_str()]);
    if ( act_client == NULL )
    {
        DbHttpProvider *p;
        if ( (p = (DbHttpProvider *)find_provider(&dbprovider)) != NULL )
        {
            setLocale();
            if ( p->check_request(this->trans->p_getDb(), act_h) )
            {
                if ( ! p->request(this->trans->p_getDb(), act_h, 1)  )
                {
                    msg.perror(E_NOTFOUND, "Provider %s unterstützt %s/%s nicht", act_h->providerpath.c_str(), act_h->dirname.c_str(), act_h->filename.c_str());
                    make_meldung();
                }
                return;
            }
        }
    }

    if (act_client != NULL)
    {
        DbHttpProvider *p;

        act_h->user = act_client->user;
        act_h->passwd = act_client->passwd;

        msg.setLang(act_client->getUserprefs("language"));
        msg.setRegion(act_client->getUserprefs("region"));

        setLocale();
        if (act_h->status == 404 )
        {
            if ( (p = (DbHttpProvider *)find_provider(&dbprovider)) != NULL )
            {
                if ( act_h->vars["sqlstart"] != "")
                    if ( act_client->db->p_getConnect()->start() )
                        if ( act_h->vars["rollback"] != "" ) act_client->db->p_getConnect()->rollback();

                if ( ! p->request(act_client->db, act_h) )
                {
                    msg.perror(E_NOTFOUND, "Provider %s unterstützt %s/%s nicht", act_h->providerpath.c_str(), act_h->dirname.c_str(), act_h->filename.c_str());
                    make_meldung();
                }
            }
            else
            {
                if ( act_h->vars.exists("asynchron") )
                    unlock_client();
                Http::make_answer();
            }
        }
    }
    else
    {
        if ( act_h->dirname == "/main/login" )
        {
            act_h->status = 404;
            Http::make_answer();
        }
        else
        {
            act_h->dirname = "/main/login";
            act_h->filename = "login.html";
            act_h->content_type = "text/html";
            act_h->setstatus = 201;
            Http::make_answer();
        }
    }
}

void DbHttp::disconnect( int client )
{
    Provider::iterator i;

    for ( i = dbprovider.begin(); i != dbprovider.end(); ++i)
        i->second->disconnect(client);

    Http::disconnect(client);

}

void DbHttp::add_provider(DbHttpProvider *p)
{
    std::string path;
    path = p->getPath();

    if (dbprovider.find(path) == dbprovider.end())
    {
        msg.pdebug(D_CON, "Provider \"%s\" wird hinzugefügt", path.c_str());
        dbprovider[path] = p;
    }
    else
    {
        msg.perror(E_PRO_EXISTS, "DbHttpProvider \"%s\" ist schon registriert", path.c_str());
    }
}

void DbHttp::del_provider(DbHttpProvider *p)
{
    std::string path;
    path = p->getPath();
    Provider::iterator i;

    msg.pdebug(D_CON, "Provider \"%s\" wird entfernt", path.c_str());
    if ((i = dbprovider.find(path)) != dbprovider.end())
    {
        dbprovider.erase(i);
    }
    else
    {
        msg.perror(E_PRO_NOT_EXISTS, "DbHttpProvider \"%s\" ist nicht registriert", path.c_str());
    }
}

int  DbHttp::check_group(HttpHeader *h, const char *group)
{
    if ( act_client == NULL )
        return false;

    DbConnect *con = this->act_client->db->p_getConnect();
    std::string stm = "SELECT t2.rolname AS loginname "
            "FROM pg_roles t0 "
              "JOIN pg_auth_members t1 ON t0.rolname = '" + std::string(group) + "'::name AND t0.oid = t1.roleid "
              "JOIN pg_roles t2 ON t1.member = t2.oid AND t2.rolname = '" + h->user + "'";

    con->execute(stm, 1);
    return con->have_result();
}

int  DbHttp::check_sysaccess(HttpHeader *h)
{
    if ( act_client == NULL )
        return false;

    DbConnect *con = this->act_client->db->p_getConnect();
    std::string stm = "SELECT DISTiNCT t0.rolname "
                "FROM pg_roles t0 "
                  "LEFT JOIN  pg_auth_members t1 ON ( t0.oid = t1.member ) "
                  "LEFT JOIN pg_roles t2 ON ( t1.roleid = t2.oid ) "
                  "JOIN mne_system.access t3 ON ( ( t0.rolname = t3.access OR t2.rolname = t3.access OR t3.access = 'user' ) "
                                               "AND t0.rolcanlogin = true "
                                               "AND NOT t0.rolname like 'mneerp%' "
                                               "AND  t0.rolname = '" + h->user + "' "
                                               "AND  t3.command = '" + h->dirname + "') ";

    con->execute(stm, 1);
    return con->have_result();
}

