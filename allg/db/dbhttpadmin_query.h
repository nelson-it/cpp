#ifndef dbhttpadmin_query_mne
#define dbhttpadmin_query_mne

#include <map>
#include <string>

#include <db/dbhttp_provider.h>

class DbHttpAdminQuery : public DbHttpProvider
{
    Message msg;

    typedef void ( DbHttpAdminQuery::*SubProvider)(Database *db, HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;
    SubProviderMap subprovider;

    std::string   ok(Database *db, HttpHeader *h);
    std::string  del(Database *db, HttpHeader *h);

    void   ok_xml(Database *db, HttpHeader *h);
    void  del_xml(Database *db, HttpHeader *h);

    void   ok_json(Database *db, HttpHeader *h);
    void  del_json(Database *db, HttpHeader *h);

protected:
    enum ERROR_TYPES
    {
	  E_SADD,
	  E_JADD,
	  E_WADD,

	  E_MAX
    };

public:
    DbHttpAdminQuery( DbHttp *h );
    virtual ~DbHttpAdminQuery();

    virtual std::string getPath() { return "db/admin/query"; }
    virtual int request (Database *db, HttpHeader *h);

};

#endif /* dbhttpadmin_query_mne */
