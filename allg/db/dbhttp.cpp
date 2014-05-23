#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <argument/argument.h>
#include <embedweb/http_provider.h>

#include "dbconnect.h"
#include "dbtranslate.h"
#include "dbtable.h"

#include "dbhttp.h"
#include "dbhttp_provider.h"

DbHttp::DbHttp(ServerSocket *s, DbHttpAnalyse *analyse, Database *db) :
Http(s, analyse, 0), msg("DbHttp")
{
    Argument a;
    char str[1024];

    this->analyse = analyse;
    this->act_client = NULL;
    this->trans = new DbTranslate(db);
#if defined(__MINGW32__) || defined(__CYGWIN__)
#else
    this->loc[a["locale"]] = this->stdloc = newlocale(LC_ALL_MASK, ((char *)a["locale"]), NULL);
#endif
    snprintf(str, sizeof(str), "MneHttpSessionId%d", (int)a["port"]);
    str[sizeof(str) - 1] = '\0';
    this->cookieid = str;

    analyse->add_http(this);
}

DbHttp::~DbHttp()
{
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
#ifdef PTHREAD
    msg.setMsgtranslator(this->trans);
#endif
}

void DbHttp::make_answer()
{
    int clear = 0;

    if (act_client == NULL)
    {
        act_client = analyse->getClient(act_h->cookies[cookieid.c_str()]);
        clear = 1;
    }

    if ( act_client == NULL )
    {
        HttpProvider *p;
        if ( (p = find_provider(&dbprovider)) != NULL )
        {
            int result = 1;
#if defined(__MINGW32__) || defined(__CYGWIN__)
            Argument a;
            setlocale(LC_ALL, (char*)a["locale"]);
#else
            uselocale(this->stdloc);
#endif
            if ( ((DbHttpProvider *) p)->check_request(this->trans->p_getDb(), act_h) )
            {
                result = ((DbHttpProvider *) p)->request(this->trans->p_getDb(), act_h, 1);
                if (!result)
                {
                    std::string str;
                    std::string::size_type pos;

                    act_h->status = 404;
                    str = meldungen[act_h->status];
                    if ((pos = str.find("#request#")) != std::string::npos)
                        str.replace(pos, 9, act_h->dirname + " " + act_h->filename);

                    fwrite(str.c_str(), str.size(), 1, act_h->content);
                }
                else if (act_h->translate)
                {
                    http_translate.make_answer(act_h, NULL);
                }
                return;
            }
        }
    }

    if (act_client != NULL)
    {
        int result = 1;
        std::map<std::string, std::string>::iterator i;
#if defined(__MINGW32__) || defined(__CYGWIN__)
#else
        std::map<std::string, locale_t>::iterator l;
#endif
        std::string lstr;
        HttpProvider *p;

        act_h->id = act_h->cookies[cookieid.c_str()];
        act_h->user = act_client->user;
        act_h->passwd = act_client->passwd;

        msg.setLang(act_client->getUserprefs("language"));
        msg.setRegion(act_client->getUserprefs("region"));

#if defined(__MINGW32__) || defined(__CYGWIN__)
        lstr = act_client->getUserprefs("mslanguage");
        setlocale(LC_ALL, lstr.c_str());
#else
        lstr = act_client->getUserprefs("language") + "_" + act_client->getUserprefs("region") + ".UTF-8";
        if ( ( l = this->loc.find(lstr)) == this->loc.end() )
        {
            this->loc[lstr] = newlocale(LC_ALL_MASK, lstr.c_str(), NULL);
            l = this->loc.find(lstr);
        }
        uselocale(l->second);
#endif

        if (act_h->status == 404 && (p = find_provider(&dbprovider)) != NULL)
        {
            result = ((DbHttpProvider *) p)->request(act_client->db, act_h);
        }
        else
        {
#ifdef PTHREAD
            if ( act_h->vars.exists("asynchron") )
                pthread_mutex_unlock(&(act_client->mutex));
#endif
            Http::make_answer();
        }

        if (!result)
        {
            std::string str;
            std::string::size_type pos;

            act_h->status = 404;
            str = meldungen[act_h->status];
            if ((pos = str.find("#request#")) != std::string::npos) str.replace(
                    pos, 9, act_h->dirname + " " + act_h->filename);

            fwrite(str.c_str(), str.size(), 1, act_h->content);
        }
        else if (act_h->translate)
        {
            http_translate.make_answer(act_h, NULL);
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

    if (clear) act_client = NULL;
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
        msg.perror(E_PRO_EXISTS, "DbHttpProvider \"%s\" ist schon registriert",
                path.c_str());
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
        msg.perror(E_PRO_NOT_EXISTS,
                "DbHttpProvider \"%s\" ist nicht registriert", path.c_str());
    }
}

