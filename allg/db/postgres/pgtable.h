#ifndef pgtable_mne
#define pgtable_mne

#if defined(__CYGWIN__) || defined(__MINGW32__)
#include <time.h>
#endif

#include <message/message.h>

#include "../dbtable.h"
#include "pgconnect.h"

class PgTable : private PgConnect, public DbTable
{
    friend class PgIndex;
    Message msg;
    std::string laststm;

protected:
      int relid;

      std::string getTypestring( const Column *col);
      std::string getDefaultstring( const Column *col);
      std::string getColumnstring( std::string name, const Column *col);

public:
    enum ERROR_TYPE
    {
        OK = 0,
	NO_TABLENAME,
	TABLE_EXISTS,
	UNKNOWN_TYPE,
	TABLE_NOTDROPED,
	COL_UNKNOWN,
	NO_COLS,
	WHERE_BLOB,
	WHERE_COLS,
	TABNAME_CHANGE,
	E_HISTORYID
    };

    enum DEBUG_TYPE
    {
        D_STM = 6
    };

    PgTable( PgConnect *con, std::string schema, std::string name);
    PgTable( PgConnect *con, std::string name);
    PgTable( PgConnect *con );

    PgTable( std::string schema, std::string name);
    PgTable( std::string name);
    PgTable();

    ~PgTable();

    void end() { PgConnect::end(); }

    void setName( std::string schema, std::string name, int ready = 0);
    void setName( std::string name, int ready = 0);

    int create( std::string schema, std::string name, ColumnMap *cols,
                int ready = 0);
    int create( std::string name, ColumnMap *cols, int ready = 0);
    int create( ColumnMap *cols, int ready = 0);
    int rename( std::string newschema, std::string newname, int ready = 0);
    int remove(std::string name = "", int ready = 0);

    int chk_column(std::string name, Column col,int modify = 0, int ready = 0 );
    int add_column(std::string name, Column col,int ready = 0 );
    int del_column(std::string name,int ready = 0 );
    int  mv_column(std::string oldname, std::string newname,int ready = 0 );

    std::string mk_where(ValueMap *where, CsList *wop = NULL);

    int insert(ValueMap *cols, int ready = 0, int ignore_error = 0);
    int modify(ValueMap *cols, ValueMap *where, int ready = 0);
    int    del(ValueMap *cols, int ready = 0);

    int check_history(int ready = 0);
    void  add_history(std::string id, int ready = 0);
    void  del_history(int ready = 0);

    DbConnect::ResultMat *select( CsList *cols,
                                  ValueMap *where,
				  CsList *wop = NULL,
				  CsList *order = NULL,
				  int distinct = 0,
				  int ready = 0);

    DbConnect::ResultMat *select( ValueMap *cols,
                                  ValueMap *where,
				  CsList *wop = NULL,
				  CsList *order = NULL,
				  int distinct = 0,
				  int ready = 0);

    std::string getLaststatement()
    {
        return laststm;
    }

};


#endif /* pgtable_mne */

