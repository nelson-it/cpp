#ifndef database_mne
#define database_mne

#include <string>
#include <set>
#include <map>

#include <string>
#include <set>
#include <map>

#include <message/message.h>

class DbConnect;
class DbCursor;
class DbJoin;
class DbQueryCreator;
class DbQuery;
class DbConstraint;

#include "dbtable.h"

class Database
{
protected:

    enum ERROR_TYPE
    {
        OK = 0,
	E_RELEASE ,
	DATABASE_MAXERROR = 100
    };

    Message msg;
    DbConnect *con;

    std::map<DbTable  *, int> tabs;
    std::set<DbCursor *> curs;
    std::set<DbJoin   *> joins;
    std::set<DbQueryCreator *> querycreators;
    std::set<DbQuery *> querys;
    std::set<DbConstraint *> constraints;

    DbCursor *last_cur;
    DbTable  *last_tab;
    DbJoin   *last_join;
    DbQueryCreator *last_querycreator;
    DbQuery *last_query;
    DbConstraint *last_constraint;

    std::string appl_schema;

public:
    Database();
    Database( std::string appl_schema);
    virtual ~Database();

    std::string getApplschema() { return appl_schema; }

    virtual Database *getDatabase() = 0;

    virtual DbConnect *p_getConnect ( std::string dbname, std::string user = "", std::string passwd = "" ) = 0;
    virtual DbConnect *p_getConnect( DbConnect *dbconnect) = 0;
    virtual DbConnect *p_getConnect () = 0;

    virtual int have_connection ();

    virtual DbCursor *p_getCursor() = 0;
    virtual DbCursor *p_getLastCursor() { return last_cur; }
    virtual void release(DbCursor *cur);

    virtual DbTable *p_getTable( std::string schema, std::string name) = 0;
    virtual DbTable *p_getTable( std::string name ) = 0;
    virtual DbTable *p_getLasttable() { return last_tab; };
    virtual int release( DbTable *tab );

    virtual DbConstraint *p_getConstraint() = 0;
    virtual DbConstraint *p_getLastconstraint() { return last_constraint; }
    virtual void release( DbConstraint *constraint );

    virtual DbJoin *p_getJoin() = 0;
    virtual DbJoin *p_getLastjoin() { return last_join; }
    virtual void release( DbJoin *join );

    virtual DbQueryCreator *p_getQuerycreator();
    virtual DbQueryCreator *p_getLastquerycreator() { return last_querycreator; }
    virtual void release( DbQueryCreator *query );

    virtual DbQuery*p_getQuery();
    virtual DbQuery*p_getLastquery() { return last_query; }
    virtual void release( DbQuery *query );

};

#endif /* database_mne */
