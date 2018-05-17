#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <time.h>

#include "pgconstraint.h"

PgConstraint::PgConstraint(PgConnect *con)
       :PgConnect(con),
        msg("PgConstraint")
{
    schema = table = "";
}

int PgConstraint::create_check( std::string schema, std::string table,
			        std::string name,   std::string check_clause,
			        std::string text_de, std::string text_en, int custom,
			        int ready)
{
    char acttime[64];
    sprintf(acttime, "%ld", time(NULL) );

    if ( schema == "" || table == "" )
    {
        msg.perror(NO_TABLENAME, "Kein Tabellenname definiert");
	return -1;
    }

    std::string cmd;
    cmd = "ALTER TABLE "
        + schema + "." + table
        + " ADD";

    if ( name != "" )
        cmd += " CONSTRAINT " + name;

    cmd += " CHECK (" + check_clause + ");";

    if ( name != "" && ( text_de != "" || text_en != "" ) )
        cmd += " INSERT INTO " + this->getApplschema() + ".tableconstraintmessages "
                   "( tableconstraintmessagesid, text_de, text_en, custom, createdate, createuser, modifydate, modifyuser )"
                 "VALUES "
                   " ( '" + name + "','" + text_de + "','" + text_en + "'," + (( custom ) ? "true" : "false") + "," + acttime + ", session_user, " + acttime + ", session_user);";

    return execute(cmd.c_str(), ready);
}

int PgConstraint::create_primary( std::string schema, std::string table,
			          std::string name, std::string cols,
			          std::string text_de, std::string text_en, int custom,
			          int ready)
{
    char acttime[64];
    sprintf(acttime, "%ld", time(NULL) );

    if ( schema == "" || table == "" )
    {
        msg.perror(NO_TABLENAME, "Kein Tabellenname definiert");
	return -1;
    }

    std::string cmd;
    cmd = "ALTER TABLE "
        + schema + "." + table
        + " ADD";

    if ( name != "" )
        cmd += " CONSTRAINT " + name;

    cmd += " PRIMARY KEY (" + cols + ");";

    if ( name != "" && ( text_de != "" || text_en != "" ) )
        cmd += " INSERT INTO " + this->getApplschema() + ".tableconstraintmessages "
                   "( tableconstraintmessagesid, text_de, text_en, custom, createdate, createuser, modifydate, modifyuser )"
                 "VALUES "
                   " ( '" + name + "','" + text_de + "','" + text_en + "'," + (( custom ) ? "true" : "false") + "," + acttime + ", session_user, " + acttime + ", session_user);";

    return execute(cmd.c_str(), ready);
}
int PgConstraint::create_foreign( std::string schema, std::string table,
			          std::string name, std::string cols,
				  std::string rschema, std::string rtable,
				  std::string rcols,
				  std::string text_de, std::string text_en, int custom,
				  int ready)
{
    char acttime[64];
    sprintf(acttime, "%ld", time(NULL) );

    if ( schema == "" || table == "" )
    {
        msg.perror(NO_TABLENAME, "Kein Tabellenname definiert");
	return -1;
    }

    std::string cmd;
    cmd = "ALTER TABLE "
        + schema + "." + table
        + " ADD";

    if ( name != "" )
        cmd += " CONSTRAINT " + name;

    cmd += " FOREIGN KEY (" + cols + ") REFERENCES ";
    cmd += rschema + "." + rtable + " ( " + rcols + ");";

    if ( name != "" && ( text_de != "" || text_en != "" ) )
        cmd += " INSERT INTO " + this->getApplschema() + ".tableconstraintmessages "
                   "( tableconstraintmessagesid, text_de, text_en, custom, createdate, createuser, modifydate, modifyuser  )"
                 "VALUES "
                   " ( '" + name + "','" + text_de + "','" + text_en + "'," + (( custom ) ? "true" : "false") + "," + acttime + ", session_user, " + acttime + ", session_user );";

    return execute(cmd.c_str(), ready);
}

int PgConstraint::remove( std::string schema, std::string table,
                          std::string name, int ready)
{

    if ( schema == "" || table == "" )
    {
        msg.perror(NO_TABLENAME, "Kein Tabellenname definiert");
	return -1;
    }

    std::string cmd;
    cmd = "ALTER TABLE "
        + schema + "." + table
        + " DROP CONSTRAINT " + name
        + "; delete from " + this->getApplschema() + ".tableconstraintmessages WHERE tableconstraintmessagesid = '" + name + "'";
    return execute(cmd.c_str(), ready);
}
