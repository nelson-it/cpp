#ifndef pgdatabase_mne
#define pgdatabase_mne

#include <string>
#include <map>

#include "../database.h"

class PgDatabase : public Database
{

public:
    enum ERROR_TYPE
    {
	CON_EXISTS = DATABASE_MAXERROR + 1,
	CON_NOTEXISTS,

	TAB_NOTDEFINED1,
	TAB_NOTDEFINED2,
	TAB_NOTDEFINED3,

	TAB_NOTEXITS1,
	TAB_NOTEXITS2,
	TAB_NOTEXITS3
    };

    PgDatabase( std::string appl_schema);
    PgDatabase();
    virtual ~PgDatabase();

    Database *getDatabase()
    {
        return new PgDatabase(appl_schema);
    }

    DbConnect *p_getConnect ( std::string dbname, std::string user = "", std::string passwd = "" );
    DbConnect *p_getConnect ( DbConnect *dbconnect );
    DbConnect *resetConnect (DbConnect *dbconnect);
    DbConnect *p_getConnect ();

    DbCursor *p_getCursor();

    DbTable *p_getTable( std::string schema, std::string name);
    DbTable *p_getTable( );

    DbConstraint *p_getConstraint();

    DbJoin  *p_getJoin ();

};

#endif /* database_mne */
