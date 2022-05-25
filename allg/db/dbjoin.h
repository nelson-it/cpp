#ifndef dbjoin_mne
#define dbjoin_mne

#include <string>

#include <utils/cslist.h>
#include "dbtable.h"

class DbJoin
{
    Message msg;
public:
    enum JOIN_TYPES
    {
        INNER = 0,
        LEFT,
        RIGHT,
        FULL,
        MAX_JOINTYPES
    };

    enum JOIN_OPPS
    {
        EQUAL,
        NOT_EQUAL,

        GREATER,
        GREATER_EQAL,

        LOWER,
        LOWER_EQUAL,

        MAX_JOINOPS
    };


protected:
    static const char* join_names[MAX_JOINTYPES];
    static const char* join_ops  [MAX_JOINOPS];

    enum ERROR_TYPES
    {
        OK = 0,
        EMPTY_TAB,
        MAX_ERROR_TYPE = 100
    };

public:
    DbJoin();
    virtual ~DbJoin() {};

    virtual void clear() = 0;
    std::string add_table( DbTable *tab, std::string pcols = "", std::string tcols = "", std::string op = "", int typ = LEFT )
    {
        if (tab->empty())
            msg.pwarning(EMPTY_TAB, "Tabelle %s exitiert nicht oder besitzt keine Spalten", tab->getFullname().c_str());

        return add_table(tab->getDbfullname(), pcols, tcols, op, typ);
    }

    virtual std::string add_table(std::string name, std::string pcols = "", std::string tcols = "", std::string op = "", int typ = LEFT ) = 0;
    virtual void back( int deepid = 0 ) = 0;
    virtual std::string getActtable() = 0;

    virtual void add_where(int notop, int leftbrace, std::string lefttab, std::string leftval, std::string op, std::string righttab, std::string rightval, int rightbrace, std::string boolop) = 0;

    virtual std::string getFrom() = 0;
    virtual std::string getWhere() = 0;

    static const char* getJoinname(int i)
    {
        if ( i >= 0 && i<MAX_JOINTYPES ) return join_names[i];
        else return "";
    }

    static std::string getJoinop(int i)
    {
        if ( i >= 0 && i<MAX_JOINOPS ) return join_ops[i];
        else { char str[24]; sprintf(str, "join%x", i ); return str; }
    }

    static std::string getJoinop(std::string op)
    {
        CsList o;
        std::string str;
        unsigned int i,j;

        o.setString(op);

        if ( o.size() > 0 )
        {
            j = CsList::Element(o[0]);
            if ( j < MAX_JOINOPS )
                str = join_ops[j];
            else
                str = "x";
        }

        for ( i = 1; i<o.size(); ++i )
        {
            j = CsList::Element(o[i]);
            if ( j < MAX_JOINOPS )
                str = str + "," + join_ops[j];
            else
                str = str + ",x";
        }

        return str;
    }
};


#endif /* dbjoin_mne */

