#ifndef dbhttpadmin_table_mne
#define dbhttpadmin_table_mne

#include <map>
#include <string>

#include <embedweb/http_provider.h>
#include <db/dbhttp_provider.h>

class DbHttpAdminTable : public DbHttpProvider
{
    Message msg;

    typedef void ( DbHttpAdminTable::*SubProvider)(Database *db, HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;
    SubProviderMap subprovider;

    DbTable::Column column_par_xml (Database *db, HttpHeader *h);

    void column_add_xml   (Database *db, HttpHeader *h);
    void column_mod_xml   (Database *db, HttpHeader *h);
    void column_del_xml   (Database *db, HttpHeader *h);

    void table_add_xml   (Database *db, HttpHeader *h);
    void table_mod_xml   (Database *db, HttpHeader *h);
    void table_del_xml   (Database *db, HttpHeader *h);

    void pkey_add_xml   (Database *db, HttpHeader *h);
    void pkey_mod_xml   (Database *db, HttpHeader *h);
    void pkey_del_xml   (Database *db, HttpHeader *h);

    void check_add_xml   (Database *db, HttpHeader *h);
    void check_mod_xml   (Database *db, HttpHeader *h);
    void check_del_xml   (Database *db, HttpHeader *h);

    void fkey_add_xml   (Database *db, HttpHeader *h);
    void fkey_mod_xml   (Database *db, HttpHeader *h);
    void fkey_del_xml   (Database *db, HttpHeader *h);

    void conrefresh(Database *db, HttpHeader *h);

protected:
    enum ERROR_TYPES
    {
        OK = 0,
	PRIMARY_INDEX,
		E_COLDUP,
		E_COLNO
    };

public:
    DbHttpAdminTable( DbHttp *h );
    virtual ~DbHttpAdminTable();

    virtual std::string getPath() { return "db/admin/table"; }
    virtual int request (Database *db, HttpHeader *h);

};

#endif /* dbhttpadmin_table_mne */
