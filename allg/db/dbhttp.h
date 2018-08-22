#ifndef dbhttp_mne
#define dbhttp_mne

#include <map>
#include <set>
#include <string>

#ifdef Darwin
#include <xlocale.h>
#endif

#include <message/message.h>
#include <embedweb/http.h>

#include "dbtranslate.h"
#include "dbhttp_analyse.h"

#include <utils/tostring.h>

class DbHttpProvider;
class DbHttpApplication;

class DbHttp : public Http
{
    Message msg;
    DbTranslate *trans;
    std::string cookieid;

#if !defined (__MINGW32__) && ! defined(__CYGWIN)
    std::map<std::string, locale_t> loc;
    locale_t stdloc;
#endif

    DbHttpAnalyse *analyse;
    DbHttpApplication *application;

    Provider dbprovider;

    DbHttpAnalyse::Client *act_client;
    void setLocale();
    void make_answer();
public:
    DbHttp(ServerSocket *s, DbHttpAnalyse *a, Database *db, int register_it = 1);
    ~DbHttp();

    void init_thread();

    void make_content(HttpHeader *h);

    void add_provider(DbHttpProvider *);
    void del_provider(DbHttpProvider *);
    void disconnect( int client );

    void unlock_client()
    {
    	if ( act_client != NULL ) act_client->unlock();
    	act_client = NULL;
    }

    DbHttpAnalyse::Client::Userprefs getUserprefs()
    {
        if ( act_client != NULL )
            return act_client->userprefs;

        DbHttpAnalyse::Client::Userprefs u;
        return u;
    }

    int check_group(HttpHeader *h, const char *group);
    int check_sysaccess(HttpHeader *h);

};

#endif /* dbhttp_mne */



