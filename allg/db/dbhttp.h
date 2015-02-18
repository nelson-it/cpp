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

class DbHttp : public Http
{
    Message msg;
    Database *dbtrans;
    DbTranslate *trans;
    std::string cookieid;

#if !defined (__MINGW32__) && ! defined(__CYGWIN)
    std::map<std::string, locale_t> loc;
    locale_t stdloc;
#endif

    DbHttpAnalyse *analyse;

    Provider dbprovider;

    DbHttpAnalyse::Client *act_client;
    void make_answer();
public:
    DbHttp(ServerSocket *s, DbHttpAnalyse *a, Database *db);
    ~DbHttp();

    void init_thread();

    void add_provider(DbHttpProvider *);
    void del_provider(DbHttpProvider *);

    void unlock_client()
    {
        #ifdef PTHREAD
    	pthread_mutex_unlock(&(act_client->mutex));
        #endif
    }

    DbHttpAnalyse::Client::Userprefs getUserprefs()
    {
        if ( act_client != NULL )
            return act_client->userprefs;

        DbHttpAnalyse::Client::Userprefs u;
        return u;
    }

    void clear_cache()
	{
		trans->clear_cache();
	}

    void read_datadir()
    {
        this->analyse->read_datadir();
    }
};

#endif /* dbhttp_mne */



