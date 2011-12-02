#ifndef dbhttputils_query_mne
#define dbhttputils_query_mne

#include <map>
#include <string>

#include <embedweb/http_provider.h>

#include "dbhttp_provider.h"

class DbHttpUtilsQuery : public DbHttpProvider
{
    Message msg;

    enum ERROR_TYPES
    {
      E_NOCOL = 1,
      E_WRONG_COLUMN,
      E_PDF_OPEN,
      E_OLD_STYLE,
      E_WVALSIZE
    };

    enum WARNING_TYPES
    {
      W_OLD = 1
    };

    typedef void ( DbHttpUtilsQuery::*SubProvider)(Database *db, HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;
    SubProviderMap subprovider;

    void data_xml (Database *db, HttpHeader *h);
public:
    DbHttpUtilsQuery( DbHttp *h );
    virtual ~DbHttpUtilsQuery();

    virtual std::string getPath() { return "db/utils/query"; }
    virtual int request (Database *db, HttpHeader *h);

};

#endif /* dbhttputils_query_mne */
