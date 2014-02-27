#ifndef dbhttp_utils_table_mne
#define dbhttp_utils_table_mne

#include <map>
#include <string>

#include <utils/cslist.h>
#include <embedweb/http_provider.h>

#include "dbhttp_provider.h"
#include "dbtable.h"

class DbHttpUtilsTable : public DbHttpProvider
{
    Message msg;

    enum ERROR_TYPES
    {
        E_WRONG_COLUMN = 1,
        E_DEL,
        E_MOD
    };

    enum WARNING_TYPES
    {
        W_OLD = 1,
	    W_XML
    };

    typedef void ( DbHttpUtilsTable::*SubProvider)(Database *db, HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;
    SubProviderMap subprovider;

    std::map<std::string, std::string> col_names;
   	std::map<std::string, long > col_dpytyps;
    std::string old_lang;

    void mk_selectcolumns( Database *db, HttpHeader *h, DbTable *tab, DbTable::ColumnVec &vals, unsigned int &anzahl_vals, DbTable::ValueMap &where, CsList &wop);

public:
    DbHttpUtilsTable( DbHttp *h, int noadd = 0 );
    virtual ~DbHttpUtilsTable();

    virtual std::string getPath() { return "db/utils/table"; }
    virtual int request (Database *db, HttpHeader *h);

    void modify (Database *db, HttpHeader *h);

    void insert_xml (Database *db, HttpHeader *h);
    void modify_xml (Database *db, HttpHeader *h);
    void delete_xml (Database *db, HttpHeader *h);
    void data_xml   (Database *db, HttpHeader *h);

    void modify_html (Database *db, HttpHeader *h);

    void file_dat(Database *db, HttpHeader *h);
};

#endif /* dbhttp_utils_table_mne */
