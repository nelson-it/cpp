#ifndef dbquery_mne
#define dbquery_mne

#include <string>
#include <map>
#include <vector>

#include <message/message.h>
#include <utils/cslist.h>

#include "dbcursor.h"
#include "dbjoin.h"

class Database;
class DbTable;

class DbQuery
{
    Message msg;

    Database *dbadmin;
    Database *db;
    DbCursor *cur;

    std::string schema;
    std::string name;
    int errorfound;

    std::string laststm;

    std::vector<DbJoin*> joins;
    std::vector<std::string> sel_fields;
    std::map<std::string, DbTable *> tables;
    std::map<int, std::string> tabnums;
    std::vector<std::vector<std::string> > sel_field;
    std::vector<std::vector<std::string> > sel_groupby;
    std::vector<std::vector<long> > sel_musthaving;
    std::vector<long> unionall;
    std::vector<long> distinct;

    std::map<std::string,long> sel_pos;
    std::vector<std::string> sel_id;
    std::vector<std::string> sel_name;
    std::vector<std::string> sel_format;
    std::vector<std::string> sel_regexp;
    std::vector<std::string> sel_regexphelp;
    std::vector<std::string> sel_regexpmod;
    std::vector<long> sel_null;
    std::vector<long> sel_typ;

    std::vector<std::string>::iterator act_sel_id;
    std::vector<std::string>::iterator act_sel_name;
    std::vector<std::string>::iterator act_sel_format;
    std::vector<std::string>::iterator act_sel_regexp;
    std::vector<std::string>::iterator act_sel_regexphelp;
    std::vector<std::string>::iterator act_sel_regexpmod;
    std::vector<long>::iterator act_sel_typ;

    std::string mk_statement(DbTable::ValueMap *w, CsList *wop, CsList *sort,
            CsList *params);
    std::string mk_statement(CsList *wcol, CsList *wval, CsList *wop,
            CsList *sort, CsList *params);

public:
    enum ERROR_TYPE
    {
        E_OK, E_TABID, E_SELID,  E_SELFIELDS, E_UNIONNUM, E_MAX = 1000
    };

    enum WARNING_TYPE
    {
        W_TABLOST = 1,
        W_NOTSTRING
    };

    DbQuery(Database *db);
    ~DbQuery();

    void setName(std::string schema, std::string name, CsList *cols, std::string unionnum = "");

    void open(CsList *wcol, CsList *wval, CsList *wops = NULL,
            CsList *sort = NULL, CsList *params = NULL);
    void open(DbTable::ValueMap *where = NULL, CsList *wops = NULL,
            CsList *sort = NULL, CsList *params = NULL);
    void close()
    {
        cur->close();
    }
    int eof()
    {
        return cur->eof();
    }

    DbConnect::ResultVec next()
    {
        return cur->next();
    }
    DbConnect::ResultVec *p_next()
    {
        return cur->p_next();
    }

    DbConnect::ResultMat *select(DbTable::ValueMap *where, CsList *wops = NULL,
            CsList *sort = NULL, CsList *params = NULL);
    DbConnect::ResultMat *select(CsList *wcol, CsList *wval,
            CsList *wops = NULL, CsList *sort = NULL,
            CsList *params = NULL);

    void start_cols();
    int getCols(std::string *id, std::string *name, long *typ,
            std::string *format = NULL, std::string *regexp = NULL,
            std::string *regexphelp = NULL, std::string *regexpmod = NULL);

    std::string::size_type find(std::string id)
    {
        std::map<std::string,long>::iterator vi;
        vi = sel_pos.find(id);
        if (vi != sel_pos.end()) return vi->second;
        else return std::string::npos;
    }
    std::string::size_type ofind(std::string id)
    {
        std::vector<std::string>::iterator vi;
        vi = std::find(sel_id.begin(), sel_id.end(), id);
        if (vi != sel_id.end()) return vi - sel_id.begin();
        else return std::string::npos;
    }
    std::string getId(unsigned int num)
    {
        if ( num < sel_id.size()) return sel_id[num];
        else return "";
    }

    std::string getName(unsigned int num)
    {
        if ( num < sel_name.size()) return sel_name[num];
        else return "";
    }

    long getColtyp(unsigned int num)
    {
        if ( num < sel_typ.size()) return sel_typ[num];
        else return DbConnect::UNKNOWN;
    }

    long getOrigcoltyp(unsigned int num)
    {
        if ( num < sel_typ.size()) return sel_typ[num];
        else return DbConnect::UNKNOWN;
    }

    std::string getFormat(unsigned int num)
    {
        if ( num < sel_format.size()) return sel_format[num];
        else return "";
    }

    std::string getRegexp(unsigned int num)
    {
        if ( num < sel_regexp.size()) return sel_regexp[num];
        else return "";
    }

    std::string getRegexphelp(unsigned int num)
    {
        if ( num < sel_regexphelp.size()) return sel_regexphelp[num];
        else return "";
    }

    std::string getRegexpmod(unsigned int num)
    {
        if ( num < sel_regexpmod.size()) return sel_regexpmod[num];
        else return "";
    }

    std::string getLaststatement()
    {
        return laststm;
    }
};

#endif /* dbquery_mne */
