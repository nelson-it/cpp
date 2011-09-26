#ifndef pgjoin_mne
#define pgjoin_mne

#include <string>
#include <vector>

#include <message/message.h>
#include "../dbjoin.h"

class PgJoin : public DbJoin
{
public:
    enum ERROR_TYPES
    {
       NOEMPTY_COLS = DbJoin::MAX_ERROR_TYPE,
       EMPTY_COLS,
       NUM_COLS
    };
    
    Message msg;

    typedef std::vector<std::string> _Join;

    std::vector<_Join> joins;
    std::vector<int>   deep_tabs;

    int tabnum;
    int joinnum;
    int backid;

    std::string where_clause;

public:
    PgJoin();
    ~PgJoin() {};

    std::string add_table( std::string name,
                   std::string pcols = "", std::string tcols = "",
           std::string op = "", int typ = LEFT );

    void add_where(int notop, int leftbrace,
		   std::string lefttab, std::string leftval,
		   std::string op,
		   std::string righttab, std::string rightval,
		   int rightbrace, std::string boolop);

    void back( int deep = 0);

    void clear()
    {
        this->joins.clear();
        this->deep_tabs.clear();
	tabnum = joinnum = backid = 0;
	where_clause = "";
    }
    std::string getActtable();
    std::string getFrom();
    std::string getWhere() { return where_clause; }

};


#endif /* pgjoin_mne */

