#ifndef dbhttputils_lang_mne
#define dbhttputils_lang_mne

#include <map>
#include <string>

#include <embedweb/http_provider.h>

#include "dbhttp_provider.h"

class DbHttpUtilsLanguage : public DbHttpProvider
{
    Message msg;

    typedef void ( DbHttpUtilsLanguage::*SubProvider)\
                              (Database *db, HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;
    SubProviderMap subprovider;

    void set (Database *db, HttpHeader *h);
    void get (Database *db, HttpHeader *h);

public:
    DbHttpUtilsLanguage( DbHttp *h );
    virtual ~DbHttpUtilsLanguage();

    virtual std::string getPath() { return "db/utils/language"; }
    virtual int request (Database *db, HttpHeader *h);

};

#endif /* dbhttputils_lang_mne */
