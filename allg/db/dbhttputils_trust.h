#ifndef dbhttp_utils_trust_mne
#define dbhttp_utils_trust_mne

#include <map>
#include <string>

#include <embedweb/http_provider.h>

#include "dbhttp_provider.h"

class DbHttpUtilsTrust : public DbHttpProvider
{
    Message msg;

    enum ERROR_TYPE
    {

        E_TYPE = 100,
        E_NOFUNC,
        E_PERM,
        E_EXEC

    };

    typedef void (DbHttpUtilsTrust::*SubProvider)(Database *db,HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;
    SubProviderMap subprovider;

    void check_user(Database *db, HttpHeader *h);
    void execute  (Database *db, HttpHeader *h, std::string name, int nologin );


public:
    DbHttpUtilsTrust( DbHttp *h );
    virtual ~DbHttpUtilsTrust();

    virtual std::string getPath() { return "db/utils/trust"; }

    virtual int check_request(Database *db, HttpHeader *h);
    virtual int request (Database *db, HttpHeader *h, int nologin );
    virtual int request (Database *db, HttpHeader *h);

};

#endif /* dbhttp_utils_trust_mne */
