#ifdef PTHREAD
#include <pthreads/pthread.h>
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

    this->analyse = analyse;
    this->act_client = NULL;
    this->trans = new DbTranslate(db);
    this->loc[a["locale"]] = this->stdloc = newlocale(LC_ALL_MASK, ((char *)a["locale"]), NULL);

    analyse->add_http(this);
}

DbHttp::~DbHttp()
{
    std::map<std::string, locale_t>::iterator i;
    delete trans;

    for ( i = this->loc.begin(); i != this->loc.end(); ++i )
        freelocale(i->second);
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
        act_client = analyse->getClient(act_h->cookies["MneHttpSessionId"]);
        clear = 1;
    }

    if ( act_client == NULL )
    {
        HttpProvider *p;
        if ( (p = find_provider(&dbprovider)) != NULL )
        {
            int result = 1;
            uselocale(this->stdloc);
            if ( ((DbHttpProvider *) p)->check_request(this->trans->p_getDb(), act_h) )
            {
                result = ((DbHttpProvider *) p)->request(this->trans->p_getDb(), act_h);
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
                return;
            }
        }
    }

    if (act_client != NULL)
    {
        int result = 1;
        std::map<std::string, std::string>::iterator i;
        std::map<std::string, locale_t>::iterator l;
        std::string lstr;
        HttpProvider *p;

        act_h->id = act_h->cookies["MneHttpSessionId"];

        msg.setLang(act_client->getUserprefs("language"));
        msg.setRegion(act_client->getUserprefs("region"));

        lstr = act_client->getUserprefs("language") + "_" + act_client->getUserprefs("region") + ".UTF-8";
        if ( ( l = this->loc.find(lstr)) == this->loc.end() )
        {
            this->loc[lstr] = newlocale(LC_ALL_MASK, lstr.c_str(), NULL);
            l = this->loc.find(lstr);
        }
        uselocale(l->second);

        if (act_h->status == 404 && (p = find_provider(&dbprovider)) != NULL)
        {
            result = ((DbHttpProvider *) p)->request(act_client->db, act_h);
        }
        else Http::make_answer();

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
    else if (act_h->status != 401)
    {
        act_h->set_cookies["MneHttpSessionId"] = "Logout";
        act_h->status = 403;
        act_h->connection = 0;
        fprintf(act_h->content, "Login ist incorrect\n");
    }
    else
    {
        fprintf(act_h->content, "Bitte einloggen\n");
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

