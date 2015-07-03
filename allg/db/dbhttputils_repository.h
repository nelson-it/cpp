#ifndef dbhttp_utils_repository_mne
#define dbhttp_utils_repository_mne

#include <map>
#include <string>

#include <embedweb/http_provider.h>
#include <embedweb/http_filesystem.h>
#include <utils/process.h>
#include "dbhttputils_query.h"
#include "dbhttputils_table.h"

class DbHttpUtilsRepository : public HttpFilesystem, DbHttpProvider
{
    Message msg;

    enum ERROR_TYPE
    {
        E_MKREPOSITORY = HttpFilesystem::E_MAX,
        E_DELREPOSITORY,
        E_REPOSITORY,

        E_ROOTNOTFOUND,
        E_FILENOTFOUND,
        E_MOD,
        E_DEL,

        E_COMMIT

    };

    enum WARNING_TYPE
    {
        W_COMMIT = HttpFilesystem::W_MAX
    };

    DbHttpUtilsQuery query;
    DbHttpUtilsTable table;

    typedef void (DbHttpUtilsRepository::*SubProvider)(Database *db,HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;
    SubProviderMap subprovider;

    std::string getRoot(HttpHeader *h);

    void read_name(Database *db, HttpHeader *h);

    int need_root;
    std::string execlog;
    std::string gitcmd;

    int exec(const CsList *cmd, const char *workdir);

    void insert_xml (Database *db, HttpHeader *h);
    void modify_xml (Database *db, HttpHeader *h);
    void delete_xml (Database *db, HttpHeader *h);

    void data_xml   (Database *db, HttpHeader *h);

    void ls       ( Database *db, HttpHeader *h);
    void log      ( Database *db, HttpHeader *h);
    void download ( Database *db, HttpHeader *h);
    void downall  ( Database *db, HttpHeader *h);

    void mkdir  ( Database *db, HttpHeader *h);
    void rmdir  ( Database *db, HttpHeader *h);

    void addfile ( Database *db, HttpHeader *h);
    void mkfile  ( Database *db, HttpHeader *h);
    void rmfile  ( Database *db, HttpHeader *h);

    void mv     ( Database *db, HttpHeader *h);

    void commit(Database *db, HttpHeader *h);

    void dbdata_update ( Database *db, HttpHeader *h);
    void dblog_update ( Database *db, HttpHeader *h);
    void dblog        ( Database *db, HttpHeader *h);


public:
    DbHttpUtilsRepository( DbHttp *h );
    virtual ~DbHttpUtilsRepository();

    virtual std::string getPath() { return "db/utils/repository"; }
    virtual int request (HttpHeader *h) { return HttpFilesystem::request(h); };
    virtual int request (Database *db, HttpHeader *h);

};

#endif /* dbhttp_utils_repository_mne */
