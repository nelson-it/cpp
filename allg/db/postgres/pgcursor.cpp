#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pgcursor.h"

int PgCursor::cur_count = 0;

PgCursor::PgCursor( std::string dbname, std::string user, std::string passwd)
         :PgConnect(dbname, user, passwd)
{
    init();
}

PgCursor::PgCursor( const PgConnect *con)
         :PgConnect(con)
{
    init();
}

PgCursor::PgCursor()
{
    init();
}

void PgCursor::init()
{
    sprintf(cur, "PgCursor%x", cur_count++);
    sprintf(fetch_cmd, "FETCH FROM %s", cur);
    opened = 0;
}

PgCursor::~PgCursor()
{
    close();
}

void PgCursor::open(char *s )
{
    char *stm;

    if ( opened ) close();
    is_eof = 0;
    stm = new char[strlen(s) + 64];
    sprintf(stm, "DECLARE %s CURSOR FOR %s", cur, s);
    execute(stm);
    delete stm;
    opened = 1;
}

DbConnect::ResultVec PgCursor::next()
{
    if ( opened ) execute(fetch_cmd);

    if ( opened && have_result() )
    {
        return get_first_result();
    }
    else
    {
        DbConnect::ResultVec v;
        opened = 0;
        return v;
    }

}

DbConnect::ResultVec* PgCursor::p_next()
{
    if ( opened ) execute(fetch_cmd);

    if ( opened && have_result() )
	return p_get_first_result();
    else
    {
	opened = 0;
        return NULL;
    }

}

void PgCursor::close()
{
    char cmd[64];

    if ( opened )
    {
	sprintf(cmd, "CLOSE %s", cur);
	execute(cmd);
	opened = 0;
    }

}

