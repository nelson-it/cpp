#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "pgdatabase.h"

#include "pgconnect.h"
#include "pgcursor.h"
#include "pgtable.h"
#include "pgjoin.h"
#include "pgconstraint.h"

PgDatabase::PgDatabase(std::string appl_schema) :
    Database(appl_schema)
{
    con = NULL;
}
PgDatabase::PgDatabase() :
    Database()
{
    con = NULL;
}

PgDatabase::~PgDatabase()
{
}

DbConnect *PgDatabase::p_getConnect(std::string dbname, std::string user,
        std::string passwd)
{
    if (con == NULL)
    {
        con = new PgConnect(dbname, user, passwd);
        ((PgConnect*)con)->setApplschema ( this->appl_schema);
    }
    else msg.pwarning(CON_EXISTS, "Verbindung besteht schon - es wird keine neue Verbindung geöfffnet");

    if (!con->have_connection())
    {
        delete con;
        con = NULL;
    }
    return con;
}

DbConnect *PgDatabase::p_getConnect()
{
    if (con == NULL) msg.perror(CON_NOTEXISTS, "Es besteht keine Verbindung zur Datenbank");
    return con;
}

DbConnect *PgDatabase::p_getConnect(DbConnect *dbconnect)
{
    if (con == NULL) con = new PgConnect((PgConnect*)dbconnect);
    else msg.pwarning(CON_EXISTS, "Verbindung besteht schon - es wird keine neue Verbindung geöfffnet");

    if (!con->have_connection())
    {
        delete con;
        con = NULL;
    }

    return con;
}

DbConnect *PgDatabase::resetConnect(DbConnect *dbconnect)
{
    if (con != NULL) delete con;

    con = new PgConnect((PgConnect*)dbconnect);
    if (!con->have_connection())
    {
        delete con;
        con = NULL;
    }

    return con;
}

DbCursor *PgDatabase::p_getCursor()
{
    last_cur = new PgCursor((PgConnect *) con);
    curs.insert(last_cur);
    return last_cur;
}

DbTable *PgDatabase::p_getTable(std::string schema, std::string name)
{
    std::map<DbTable *, int>::iterator i;
    for (i = tabs.begin(); i != tabs.end(); ++i)
        if (i->first->getSchema() == schema && i->first->getName() == name) break;

    if (i != tabs.end())
    {
        last_tab = i->first;
        i->second = i->second + 1;
        last_tab->setName(schema, name);
    }
    else
    {
        last_tab = new PgTable((PgConnect *) con, schema, name);
        tabs[last_tab] = 1;
    }

    return last_tab;
}

DbTable *PgDatabase::p_getTable()
{
    last_tab = new PgTable((PgConnect *) con);
    tabs[last_tab] = 1;
    return last_tab;
}

DbConstraint *PgDatabase::p_getConstraint()
{
    last_constraint = new PgConstraint((PgConnect*) con);
    constraints.insert(last_constraint);
    return last_constraint;
}

DbJoin *PgDatabase::p_getJoin()
{
    last_join = new PgJoin;
    joins.insert(last_join);
    return last_join;
}

