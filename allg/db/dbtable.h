#ifndef dbtable_mne
#define dbtable_mne

#include <string>
#include <vector>
#include <map>

#include <utils/cslist.h>
#include <message/message.h>

#include "dbconnect.h"

class DbTable
{
    Message msg;
    enum ERROR_TYPES
    {
        OK = 0,
	NO_VALUES
    };

public:
    class Value
    {
    public:
         std::string value;
	 char *buffer;
	 int size;

	 Value();
	 Value( const Value &in);
	 Value &operator=(const Value &in);

	 Value(const char *v);
	 Value(char *v);
	 Value(char *v, int size);  // Large Objects
	 Value(std::string v);
	 Value(int v);
	 Value(long v);
	 Value(double v);

	 ~Value();
    };

    class Column
    {
	friend class DbTable;

        int have_value;

    public:
	enum COLTYPE_MASK
	{
	   DBDEF  = 1,
	   DBNULL = 2
	};

	enum NULL_TYPE
	{
	    NOTNULL_NOTDEF = 0,
	    NOTNULL_DEF,
	    NULL_NOTDEF,
	    NULL_DEF
	};

	enum DIFF_TYPE
	{
	    D_OK     = 0,
	    D_TYP    = 1,
	    D_SIZE   = 2,
	    D_NULL   = 4,
	    D_VALUE  = 8
	};

	std::string name;

    int int_typ;
    int typ;
	int size;
	int can_null;

	std::string value;

	std::map<std::string,std::string> text;

	std::string regexp;
    std::map<std::string,std::string> regexphelp;

    long dpytyp;
    int num;

        Column();
        Column(std::string name);
        Column(std::string name, int typ, std::string value = "",
	       int can_null = Column::NOTNULL_DEF, std::string reg_exp = "",
	       long dpytyp = -1);
        Column(std::string name, int typ, int size, std::string value = "",
	       int can_null = Column::NOTNULL_DEF, std::string reg_exp = "",
	       long dpytyp = -1);

	std::string styp( int typ = -1 );
    };

    class TableName
    {
    public:
        std::string schema;
	std::string name;
	std::string typ;
    };

    typedef std::vector<TableName> TableVec;
    typedef std::vector<Column> ColumnVec;
    typedef std::map<std::string,Column> ColumnMap;
    typedef std::map<std::string, Value> ValueMap;
    typedef std::map<std::string, ColumnMap> ColumnMapAll;

protected:
    DbConnect *dbconnect;

    std::string schema;
    std::string name;
    char        typ;
    int have_usertime_columns;

    static ColumnMapAll all_cols;
    static std::map<std::string, char> all_typ;

    ColumnMap cols;

    virtual void start_columncheck();
    virtual int columncheck();
    void ok_columncheck(std::string name)
        { cols[name].have_value = 1; }

public:
    DbTable() : msg("DbTable")
    { have_usertime_columns = 0; }
    virtual ~DbTable() {};

    int empty() { return cols.empty(); }

    virtual void end() = 0;

    virtual std::string getValue( const Column *col );

    virtual void setName(std::string name, int ready = 0) = 0;
    virtual void setName(std::string schema, std::string name, int ready=0) = 0;

    virtual int create(std::string schema, std::string name,
                       ColumnMap *cols, int ready = 0)=0;
    virtual int create(std::string name, ColumnMap *cols, int ready = 0) = 0;
    virtual int create(ColumnMap *cols, int ready = 0) = 0;
    virtual int rename(std::string newschema, std::string newname, int ready = 0) = 0;
    virtual int remove(std::string name, int ready = 0) = 0;

    virtual int mk_usertime_columns();
    virtual int chk_usertime_columns() { return have_usertime_columns; }

    virtual int chk_column(std::string name, Column col, int modify = 0,
                           int ready = 0) = 0;
    virtual int add_column(std::string name, Column col, int ready = 0) = 0;
    virtual int del_column(std::string name, int ready = 0) = 0;
    virtual int  mv_column(std::string oldname, std::string newname,
                           int ready = 0) = 0;

    virtual int check_history(int ready = 0) = 0;
    virtual void  add_history(std::string id, int ready = 0) = 0;
    virtual void  del_history(int ready = 0) = 0;
    virtual std::string getLaststatement() = 0;

    char getType()          { return this->typ; }
    std::string getSchema() { return schema; }

    std::string getName() { return name; }
    std::string getFullname() { return schema + "." + name; }

    virtual std::string getDbname() { return name; }
    virtual std::string getDbfullname() { return schema + "." + name; }

    void del_allcolumns()
    {
        ColumnMapAll::iterator a;
	if ( ( a = all_cols.find(schema + "." + name)) != all_cols.end() )
	  all_cols.erase(a);
    }

    ColumnMap    getColumns() { return cols; }
    ColumnMap* p_getColumns() { return &cols; }
    Column       getColumn(std::string name);

    virtual std::string mk_where(ValueMap *where, CsList *wop) = 0;

    virtual int insert(ValueMap *cols, int ready = 0, int ignore_error = 0) = 0;
    virtual int modify(ValueMap *cols, ValueMap *where, int ready = 0) = 0;
    virtual int    del(ValueMap *where, int ready = 0) = 0;

    virtual DbConnect::ResultMat* select( CsList *cols,
                                          ValueMap *where,
					  CsList *wop = NULL,
					  CsList *order = NULL,
					  int distinct = 0,
					  int ready = 0) = 0;

    virtual DbConnect::ResultMat* select(ValueMap *cols,
                                         ValueMap *where,
					 CsList *wop = NULL,
					 CsList *order = NULL,
					 int distinct = 0,
					 int ready = 0) = 0;

    virtual DbConnect::ResultMat* select(std::vector<std::string> *cols,
                                         ValueMap *where = NULL,
					 CsList *wop = NULL,
					 std::vector<std::string> *order = NULL,
					 int distinct = 0,
					 int ready = 0)
    {
        CsList l(cols);
	if ( order == NULL )
	    return select(&l, where, wop, (CsList *)NULL, distinct, ready);

	CsList o(order);
	return select(&l, where, wop, &o, distinct, ready);

    }

    virtual DbConnect::ResultMat* select(ColumnVec *cols,
                                         ValueMap *where = NULL,
					 CsList *wop = NULL,
					 CsList *order = NULL,
					 int distinct = 0,
					 int ready = 0)
    {
        std::vector<std::string> c;
	ColumnVec::iterator ci;
	for ( ci = cols->begin(); ci != cols->end(); ++ci )
	    c.push_back(ci->name);

	return select(&c, where, wop, order, distinct, ready);
    }

    virtual DbConnect::ResultMat* select(std::vector<std::string> *cols,
                                         ValueMap *where,
					 CsList *wop,
					 CsList *order,
					 int distinct = 0,
					 int ready = 0)
    {
        CsList l(cols);
	return select(&l, where, wop, order, distinct, ready);
    }

    virtual DbConnect::ResultMat* select(CsList *cols,
                                         ValueMap *where,
					 CsList *wop,
					 std::vector<std::string> *order,
					 int distinct = 0, int ready = 0)
    {
	if ( order == NULL )
	    return select(cols, where, wop, (CsList *)NULL, distinct, ready);

	CsList o(order);
	return select(cols, where, wop, &o, distinct, ready);
    }

    virtual DbConnect::ResultMat* select(ValueMap *cols,
                                         ValueMap *where,
					 CsList *wop,
					 std::vector<std::string> *order,
					 int distinct = 0, int ready = 0)
    {
	if ( order == NULL )
	    return select(cols, where, wop, (CsList *)NULL, distinct, ready);

	CsList o(order);
	return select(cols, where, wop, &o, distinct, ready);
    }
};

#endif /* dbtable_mne */
