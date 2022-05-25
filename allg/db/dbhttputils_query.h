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
      E_WVALSIZE,

      E_DYNDATA_COLSSIZE
    };

    enum WARNING_TYPES
    {
      W_OLD = 1,
      W_CONV
    };

    void mk_export(HttpHeader *h);
    void mk_exportvalue(HttpHeader *h, DbConnect::Result r, int dpytyp, std::string colfs, std::string sep, std::string dateformat);

    typedef void ( DbHttpUtilsQuery::*SubProvider)(Database *db, HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;
    SubProviderMap subprovider;

protected:
    static std::map<std::string,std::string> dateformat;
    static std::map<std::string,std::string> days;

public:
    DbHttpUtilsQuery( DbHttp *h, int noadd = 0 );
    virtual ~DbHttpUtilsQuery();

    virtual std::string getPath() { return "db/utils/query"; }
    virtual int request (Database *db, HttpHeader *h);

    void dyndata_xml (Database *db, HttpHeader *h);
    void data_xml (Database *db, HttpHeader *h);
    void data_csv (Database *db, HttpHeader *h);
    void data_json (Database *db, HttpHeader *h);

};

#endif /* dbhttputils_query_mne */
