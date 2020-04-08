#ifndef dbhttp_compose_mne
#define dbhttp_compose_mne

#include <map>
#include <string>

#include <db/dbhttputils_query.h>

class DbHttpCompose : public DbHttpUtilsQuery
{
    Message msg;

    typedef void ( DbHttpCompose::*SubProvider)(Database *db, HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;
    SubProviderMap subprovider;

    void   weblet_json(Database *db, HttpHeader *h);
    void   subweblets_json(Database *db, HttpHeader *h);
    void   select_json(Database *db, HttpHeader *h);

    enum ERROR_TYPES
    {
    };

    enum WARNING_TYPES
    {
    };

public:
    DbHttpCompose( DbHttp *h );
    virtual ~DbHttpCompose();

    virtual std::string getPath() { return "htmlcompose"; }
    virtual int request (Database *db, HttpHeader *h);

};

#endif /* dbhttp_compose_mne */
