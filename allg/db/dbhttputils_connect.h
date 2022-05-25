#ifndef dbhttp_utils_connect_mne
#define dbhttp_utils_connect_mne

#include <map>
#include <string>

#include <embedweb/http_provider.h>

#include "dbhttp_analyse.h"
#include "dbhttp_provider.h"

class DbHttpUtilsConnect : public DbHttpProvider
{
    Message msg;
    DbHttpAnalyse *analyse;

    enum ERROR_TYPE
    {

        E_TYPE = 100

    };

    typedef void (DbHttpUtilsConnect::*SubProvider)(Database *db,HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;
    SubProviderMap subprovider;

    void start_xml        (Database *db, HttpHeader *h);
    void end_xml          (Database *db, HttpHeader *h);
    void reload_xml       (Database *db, HttpHeader *h);

    void start_json        (Database *db, HttpHeader *h);
    void end_json          (Database *db, HttpHeader *h);
    void reload_json       (Database *db, HttpHeader *h);

    void sql_execute_xml      (Database *db, HttpHeader *h);
    void sql_execute_json     (Database *db, HttpHeader *h);

    void func_execute_xml     (Database *db, HttpHeader *h);
    void func_mod_xml         (Database *db, HttpHeader *h);
    void func_del_xml         (Database *db, HttpHeader *h);

    void func_execute_json    (Database *db, HttpHeader *h);
    void func_mod_json        (Database *db, HttpHeader *h);
    void func_del_json        (Database *db, HttpHeader *h);

    std::string func_execute    (Database *db, HttpHeader *h);
    int func_mod                (Database *db, HttpHeader *h);
    int func_del                (Database *db, HttpHeader *h);

public:
    DbHttpUtilsConnect( DbHttp *h, DbHttpAnalyse *a );
    virtual ~DbHttpUtilsConnect();

    virtual std::string getPath() { return "db/utils/connect"; }

    virtual int request (Database *db, HttpHeader *h);

};

#endif /* dbhttp_utils_connect_mne */
