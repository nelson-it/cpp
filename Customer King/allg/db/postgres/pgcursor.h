#ifndef pgcursor_mne
#define pgcursor_mne

#include "../dbcursor.h"
#include "pgconnect.h"

class PgCursor : public PgConnect, public DbCursor
{
     static int cur_count;
     char cur[32];
     char fetch_cmd[64];
     int opened;

     void init();

public:
     PgCursor(std::string dbname, std::string user="", std::string passwd="");
     PgCursor(const PgConnect *con);
     PgCursor();

     ~PgCursor();

     void open( char *stm);
     void close();
     int eof() { return PgConnect::eof(); }

     void start() { PgConnect::start(); }
     void   end() { PgConnect::end(); }
     DbConnect::ResultVec next();
     DbConnect::ResultVec* p_next();

};


#endif /* pgcursor_mne */

