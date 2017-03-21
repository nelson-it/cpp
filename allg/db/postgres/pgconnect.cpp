#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#ifdef Darwin
#include <xlocale.h>
#endif

#include <libpq/libpq-fs.h>

#include <argument/argument.h>
#include <message/message.h>
#include <utils/tostring.h>

#include "../dbconstraint_error.h"

#include "pgtypes.h"
#include "pgconnect.h"

static void NoticeProcessor(void *arg, const char *message)
{
    ((PgConnect*) arg)->notice(message);
}

std::map<PGconn*, PgConnect::Connections> PgConnect::connections;
PGconn *PgConnect::last_con = NULL;

std::map<long, long> PgConnect::pgbasetypes;

PgConnect::PgConnect(std::string dbname, std::string user, std::string passwd)
{

    Argument a;
    const char *db;
    std::string argdb(a["DbDatabase"]);

    if (dbname == "")
    {
        db = argdb.c_str();
    }
    else
    {
        db = (char*) dbname.c_str();
    }

    con = NULL;
    ignore_error = is_eof = ignore_oid = 0;
    p_errstr = errstr = errresult = NULL;

    open_connection(db, user.c_str(), passwd.c_str());
    msg.pdebug(D_CON, "öffne Verbindung %x %d", this->con, connections[this->con].anzahl_connect);
}

PgConnect::PgConnect(const PgConnect *con)
{
    if (con != NULL)
        this->con = con->con;
    else
        this->con = last_con;

    this->extension = connections[this->con].extension;
    connections[this->con].anzahl_connect++;
    msg.pdebug(D_CONCOUNT, "neue Verbindung %x %d", this->con, connections[this->con].anzahl_connect);

    ignore_error = is_eof = ignore_oid = 0;
    p_errstr = errstr = errresult = NULL;

}

PgConnect::PgConnect()
{
    con = last_con;

    this->extension = connections[this->con].extension;
    connections[this->con].anzahl_connect++;
    msg.pdebug(D_CONCOUNT, "neue Verbindung %x %d", this->con, connections[this->con].anzahl_connect);

    ignore_error = is_eof = ignore_oid = 0;
    p_errstr = errstr = errresult = NULL;
}

PgConnect::~PgConnect()
{
    if (con != NULL)
    {
        msg.pdebug(D_CONCOUNT, "lösche Verbindung %d", connections[this->con].anzahl_connect);
        connections[this->con].anzahl_connect--;
        if (connections[this->con].anzahl_connect == 0)
            close_connection();
    }
}

void PgConnect::open_connection(const char *dbname, const char *user, const char *passwd)
{
    char *db;
    char *host;
    char *port;
    char coninfo[10240];
    char *c1;
    const char *c2;
    int last;

    //start(0);

    if (user != NULL && *user == '\0')
        user = NULL;
    if (passwd != NULL && *passwd == '\0')
        passwd = NULL;

    db = new char[strlen(dbname) + 1];
    host = new char[strlen(dbname) + 1];
    port = new char[strlen(dbname) + 1];

    for (c1 = db, c2 = dbname; *c2 != '\0' && *c2 != '@' && *c2 != ':'; *c1++ = *c2++)
        ;
    *c1 = '\0';

    if (*c2 == '@')
        c2++;
    for (c1 = host; *c2 != '\0' && *c2 != ':'; *c1++ = *c2++)
        ;
    *c1 = '\0';

    if (*host == '\0')
    {
        delete[] host;
        host = NULL;
    }

    if (*c2 == ':')
        c2++;
    for (c1 = port; *c2 != '\0'; *c1++ = *c2++)
        ;
    *c1 = '\0';

    if (*port == '\0')
    {
        delete[] port;
        port = NULL;
    }

    coninfo[0] = '\0';
    if (host != NULL)
        snprintf(&coninfo[strlen(coninfo)], sizeof(coninfo) - strlen(coninfo), "host = %s ", host);
    if (port != NULL)
        snprintf(&coninfo[strlen(coninfo)], sizeof(coninfo) - strlen(coninfo), "port = %s ", port);
    if (db != NULL)
        snprintf(&coninfo[strlen(coninfo)], sizeof(coninfo) - strlen(coninfo),"dbname = %s ", db);
    if (user != NULL)
        snprintf(&coninfo[strlen(coninfo)], sizeof(coninfo) - strlen(coninfo),"user = %s ", user);
    last = strlen(coninfo);
    if (passwd != NULL)
        snprintf(&coninfo[strlen(coninfo)], sizeof(coninfo) - strlen(coninfo),"password = %s ", passwd);

    last_con = con = PQconnectdb(coninfo);

    if (con != NULL)
    {

        if (PQstatus(con) != CONNECTION_OK)
        {
            coninfo[last] = '\0';
            con = last_con = NULL;
            msg.perror(E_CONNECTION_NOT_OK,
                    "Konnte mich nicht zur Datenbank %s verbinden", coninfo);
            msg.line("Grund: %s", mk_error());

            char *str;
            while ((str = mk_error()) != NULL)
            {
                msg.ignore_lang = 1;
                msg.line("%s", str);
            }

            close_connection();
        }
        else
        {
            connections[this->con].anzahl_connect = 1;
            PQsetNoticeProcessor(con, NoticeProcessor, this);
            start(0);
        }
    }
    else
    {
        //connections[con].error_found = 1;
        msg.perror(E_CONNECTION_NULL, "PQsetdbLogin gibt NULL zurück");
    }

    if (port != NULL)
        delete[] port;
    if (host != NULL)
        delete[] host;
    delete[] db;

    if (con != NULL)
    {
        unsigned int i;
        end();

        execute("select relname "
                "from pg_catalog.pg_class c, pg_catalog.pg_namespace n "
                "where c.relnamespace = n.oid "
                "and n.nspname = 'mne_catalog' "
                "and c.relname = 'blobcols' "
                "and c.relkind = 'r'");

        extension = (!result.empty());
        connections[con].extension = extension;
        end();

        if ( pgbasetypes.empty() )
        {
            execute("select oid, typbasetype from pg_catalog.pg_type where typbasetype != 0");
            for ( i = 0; i<result.size(); i++ )
            {
                pgbasetypes[result[i][0]] = result[i][1];
            }

            end();
        }

    }
}

void PgConnect::close_connection()
{
    if (con != NULL)
    {
        PQfinish(con);
        msg.pdebug(D_CONCOUNT, "Verbindung endgültig gelöscht");
        connections.erase(con);
    }

    con = NULL;

}

void PgConnect::mk_result(PGresult *res, const char *stm)
{

    int i, k, rows, cols;
    Oid typ;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::string loc = setlocale(LC_NUMERIC, NULL);
    setlocale(LC_NUMERIC, "C");
#else
    locale_t loc;
    loc = newlocale(LC_NUMERIC_MASK, "C", NULL);
    loc = uselocale(loc);
#endif
    result.clear();

    is_eof = (rows = PQntuples(res)) == 0;
    cols = PQnfields(res);

    for (k = 0; k < rows; k++)
    {
        ResultVec rv;

        for (i = 0; i < cols; i++)
        {
            Result r;

            typ = PQftype(res, i);

            r.typ = int2ext_pgtype(typ);
            r.isnull = PQgetisnull(res, k, i);

            switch (typ)
            {
            case BPCHAROID:
            case CHAROID:
            case VARCHAROID:
            case TEXTOID:
            case TIMESTAMPOID:
            case NAMEOID:
            case INT2VECTOROID:
            case ACLITEMOID:
            case UNKNOWNOID:
            {
                if (!r.isnull)
                {
                    r.length = PQgetlength(res, k, i) + 1;
                    r.value = (void *) new char[r.length];
                    memcpy(r.value, PQgetvalue(res, k, i), r.length - 1);
                    ((char*) r.value)[r.length - 1] = '\0';
                }
                else
                {
                    r.length = 1;
                    r.value = new char[1];
                    ((char*) r.value)[0] = '\0';
                }

                break;
            }

            case BOOLOID:
            {

                r.length = sizeof(long);
                r.value =  new char[r.length];
                //r.value = (void *) new long;
                if (!r.isnull)
                    *(long*) r.value = *PQgetvalue(res, k, i) == 't' ? 1 : 0;
                else
                    *(long*) r.value = 0;

                break;
            }

            case INT8OID:
            case INT4OID:
            case INT2OID:
            {

                r.length = sizeof(long);
                r.value =  new char[r.length];
                //r.value = (void *) new long;
                if (!r.isnull)
                    *(long*) r.value = strtol(PQgetvalue(res, k, i), NULL, 10);
                else
                    *(long*) r.value = 0;

                break;
            }

            case OIDOID:
            {

                if (!r.isnull)
                {
                    Oid id;
                    int fd;

                    id = strtol(PQgetvalue(res, k, i), NULL, 10);
                    if (0 && ignore_oid == 0 && (fd
                            = lo_open(con, id, INV_READ)) >= 0)
                    {
                        r.length = lo_lseek(con, fd, 0, SEEK_END);
                        r.value = (void *) new char[r.length + 1];
                        ((char *) r.value)[r.length] = '\0';
                        lo_lseek(con, fd, 0, SEEK_SET);
                        lo_read(con, fd, (char *) r.value, r.length);
                        lo_close(con, fd);
                    }
                    else
                    {
                        r.length = sizeof(long);
                        r.value =  new char[r.length];
                        //r.value = (void *) new long;
                        *(unsigned long*) r.value = id;
                        r.typ = LONG;
                    }
                }
                else
                {
                    r.length = sizeof(long);
                    r.value =  new char[r.length];
                    //r.value = (void *) new long;
                    *(unsigned long*) r.value = 0;
                    r.typ = LONG;
                }

                break;
            }

            case FLOAT4OID:
            case FLOAT8OID:
            {

                r.length = sizeof(double);
                r.value =  new char[r.length];
                //r.value = (void *) new double;
                if (!r.isnull)
                    *(double*) r.value = strtod(PQgetvalue(res, k, i), NULL);
                else
                    *(double*) r.value = 0;

                break;
            }

            default:
            {

                char *c;

                r.typ = CHAR;
                if (!r.isnull)
                {
                    msg.perror(E_TYPE_UNKNOWN,
                            "Der Typ %d von Spalte %s ist unbekannt", typ,
                            PQfname(res, i));
                    msg.line("für Sql-Befehl");
                    msg.ignore_lang = 1;
                    msg.line("%s", stm);

                    r.length = PQgetlength(res, k, i) - 1;
                    for (c = PQgetvalue(res, k, i); r.length > 0; r.length--)
                        if (!isspace(c[r.length]))
                            break;

                    r.length += 2;
                    r.value = (void *) new char[r.length];
                    memcpy(r.value, PQgetvalue(res, k, i), r.length - 1);
                    ((char*) r.value)[r.length - 1] = '\0';
                }
                else
                {
                    r.length = 1;
                    r.value = (void *) new char[1];
                    *(char*) r.value = '\0';
                }
                break;
            }
            }
            rv.push_back(r);
        }
        result.push_back(rv);
    }

#if defined(__MINGW32__) || defined(__CYGWIN__)
    setlocale(LC_NUMERIC, loc.c_str());
#else
    loc = uselocale(loc);
    freelocale(loc);
#endif
}

char *PgConnect::mk_error(PGresult *res)
{
    int i;
    char *c;

    if (errstr == NULL)
    {
        if ( res != NULL )
        {
            std::string status = PQresultErrorField(res, PG_DIAG_SQLSTATE);
            if ( status == "23505" || status == "23514" || status == "23503" )
            {
                DbConstraintError err;

                std::string::size_type i,j;
                std::string s;
                std::string f;
                std::string str = PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY);

                for ( j=0,i=0; i != std::string::npos && j != std::string::npos; )
                {
                    i = str.find("»", j);
                    j = str.find("«", i);
                    f = str.substr(i+2, j - i - 2);
                    if ( ( s = err.get(msg.getLang(), f)) != "" ) break;
                }

                if ( s == "" )
                {
                    for ( j=0,i=0; i != std::string::npos && j != std::string::npos; )
                    {
                        i = str.find("\"", j);
                        j = str.find("\"", i + 1);
                        f = str.substr(i+1, j - i - 1);
                        if ( ( s = err.get(msg.getLang(), f)) != "" ) break;
                        j++;
                    }
                }

                if ( s == "" )
                {
                    p_errstr = errstr = new char[strlen(PQerrorMessage(con)) + 1];
                    errresult = new char[strlen(PQerrorMessage(con)) + 1];
                    strcpy(errstr, PQerrorMessage(con));
                }
                else
                {
                    unsigned int size = strlen(s.c_str()) + 1;
                    p_errstr = errstr = new char[size];
                    errresult = new char[size];
                    strcpy(errstr, s.c_str());
                }
            }
            else
            {
                p_errstr = errstr = new char[strlen(PQerrorMessage(con)) + 1];
                errresult = new char[strlen(PQerrorMessage(con)) + 1];
                strcpy(errstr, PQerrorMessage(con));
            }
        }
        else
        {
            p_errstr = errstr = new char[strlen(PQerrorMessage(con)) + 1];
            errresult = new char[strlen(PQerrorMessage(con)) + 1];
            strcpy(errstr, PQerrorMessage(con));
        }
    }

    for (i = 0; *p_errstr != '\n' && *p_errstr != '\r' && *p_errstr != '\0'; i++)
        errresult[i] = *p_errstr++;
    errresult[i] = '\0';

    while ( ( *p_errstr == '\n' || *p_errstr == '\r' ) && *p_errstr != '\0')
        p_errstr++;


    if ((c = strstr(errresult, "#mne_lang#")) != NULL)
    {
        char *cc;
        c[0] = '\0';
        cc = &c[10];
        while (*cc != '\0' && *cc != '#')
            cc++;
        *cc = '\0';
        if (*cc == '#')
            cc++;

        errlangstr = errresult + msg.get(&c[10]) + cc;
        return (char *) errlangstr.c_str();
    }

    if (*errresult == '\0')
    {
        delete[] errstr;
        delete[] errresult;

        errstr = NULL;
        errresult = NULL;
        return NULL;
    }

    return errresult;
}

std::string PgConnect::getValue(int typ, std::string str)
{
    if (str == "current_user" || str == "session_user" || str == "'session_user'" || str == "'current_user'" || str == "current_database()"|| str == "'current_database()'")
        return str;
    else
        return DbConnect::getValue(typ, str);
}

int PgConnect::add_schema(std::string schema, int ready)
{
    return DbConnect::execute("CREATE SCHEMA " + schema, ready);
}

int PgConnect::del_schema(std::string schema, int ready)
{
    return DbConnect::execute("DROP SCHEMA " + schema + " CASCADE ", ready);
}

std::vector<std::string> PgConnect::getSchemas()
{
    std::vector<std::string> r;
    unsigned int i;

    execute("select nspname from pg_namespace "
            "where nspname <> 'information_schema' AND "
            "      nspname <> 'mne_catalog' AND "
            "  NOT nspname like 'pg%' ");

    for (i = 0; i < result.size(); i++)
        r.push_back((char *) result[i][0].value);

    return r;
}

int PgConnect::setCurschema(std::string schema, int ready)
{
    execute("select current_schema()");

    if (schema != (char *) result[0][0].value)
    {
        std::string path;
        std::string::size_type i;

        execute(("select nspname from pg_namespace where nspname = '" + schema
                + "'").c_str());
        if (result.empty())
        {
            msg.perror(E_UNKNOWN_SCHEMA, "Schema %s existiert nicht\n",
                    schema.c_str());
            if (ready)
                end();
            return -1;
        }

        execute("SHOW search_path");
        path = (char *) result[0][0].value;

        if ((i = path.find("$user")) != std::string::npos)
        {
            execute("select nspname from pg_namespace where "
                    "nspname = current_user");

            if (result.empty())
                path = path.replace(i, 5, "");
            else
                path = path.replace(i, 5, (char *) result[0][0].value);
        }

        if ((i = path.find(schema)) != std::string::npos)
            path = path.replace(i, schema.size(), "");

        path = schema + "," + path;
        if ((i = path.find(",,")) != std::string::npos)
            path = path.replace(i, 1, "");

        execute(("SET search_path TO " + path).c_str());
    }

    int ret = -(get_error() + get_warning());

    if (ready)
        end();

    return ret;
}

std::string PgConnect::getCurschema(int ready)
{
    execute("select current_schema()", ready);

    if (!result.empty())
        return (char*) result[0][0].value;
    else
        return "unbekannt";
}

int PgConnect::execute(const char *stm, int ready, int no_clearresult)
{
    PGresult *res;
    ExecStatusType status;
    int r;
    std::map<PGconn*, std::string>::iterator it;

    is_eof = 1;
    if (!no_clearresult)
        result.clear();

    if (con == NULL)
    {
        msg.perror(E_EXECUTE_CON_NULL, "Keine Verbindung zur Datenbank:");
        msg.line("für Sql-Befehl");
        msg.ignore_lang = 1;
        msg.line("%s", stm);

        return -1;
    }

    msg.pdebug(D_STM, "%s", (char *) stm);

#ifdef PTHREAD
        pthread_mutex_lock(&connections[con].mutex);
#endif

    if ( connections[con].in_transaction == "" )
    {
        res = PQexec(con, "BEGIN");
        if ( res != NULL ) PQclear(res);
    }

    connections[con].in_transaction = connections[con].in_transaction + stm + "\n";

    res = PQexec(con, (char *) stm);

    status = PQresultStatus(res);
    switch (status)
    {
    case PGRES_EMPTY_QUERY: // 0
        break;

    case PGRES_COMMAND_OK: // 1
        break;

    case PGRES_TUPLES_OK: // 2
        if (no_clearresult)
        {
            msg.pwarning(E_EXECUTE_NORESULT, "Ignoriere Ergebnis für");
            msg.ignore_lang = 1;
            msg.line("%s", stm);
        }
        else
            mk_result(res, stm);
        break;

    case PGRES_COPY_OUT: // 3
    case PGRES_COPY_IN: // 4
        break;

    case PGRES_BAD_RESPONSE: // 5
    case PGRES_FATAL_ERROR: // 7
        if (!ignore_error)
        {
            char *str;

            connections[con].error_found = 1;

            msg.ignore_lang = 1;
            msg.perror(E_EXECUTE_ERROR, "%s", mk_error(res));
            while ((str = mk_error()) != NULL)
            {
                msg.ignore_lang = 1;
                msg.line("%s", str);
            }
            msg.line("für Sql-Befehl");
            msg.ignore_lang = 1;
            msg.line("%s", stm);
        }
        break;

    case PGRES_NONFATAL_ERROR: // 6
        if (!ignore_error)
        {
            char *str;
            connections[con].warning_found = 1;

            msg.ignore_lang = 1;
            msg.pwarning(E_EXECUTE_WARNING, "%s", mk_error(res));
            while ((str = mk_error()) != NULL)
            {
                msg.ignore_lang = 1;
                msg.line("%s", str);
            }
            msg.line("für Sql-Befehl");
            msg.ignore_lang = 1;
            msg.line("%s", stm);
        }
        break;
    default:
        connections[con].warning_found = 1;
        msg.perror(E_EXECUTE_STATUS, "unbekannter Status %d", status);
        msg.line("für Sql-Befehl");
        msg.ignore_lang = 1;
        msg.line("%s", stm);
        break;
    }

    PQclear(res);

    if (connections[con].error_found || connections[con].warning_found)
        r = -1;
    else
        r = 0;

#ifdef PTHREAD
    pthread_mutex_unlock(&connections[con].mutex);
#endif

    if (ready)
        this->end();

    return r;
}

void PgConnect::notice(const char*message)
{
    char *c;
    char *cc;

    std::string langstr;
    if (!connections[con].ignore_message)
    {
        if ((c = strstr((char*)message, "#mne_lang#")) != NULL)
        {
            cc = &c[10];
            while (*cc != '\0' && *cc != '\n' &&*cc != '\r' && *cc != '#')
                cc++;
            *cc = '\0';

            langstr = msg.get(&c[10]);
            c = (char *) langstr.c_str();
        }
        else
        {
            cc = c = (char *) message;
            while (*cc != '\0' && *cc != '\n' &&*cc != '\r' )
                cc++;
            *cc = '\0';
        }
        msg.pmessage(-1, "%s", c);
    }
}

void PgConnect::commit()
{
    I_MES(execute("COMMIT", 0, 1));
    connections[con].in_transaction = "";
}

void PgConnect::rollback()
{
    I_MES(execute("ROLLBACK", 0, 1));
    connections[con].in_transaction = "";
}

int PgConnect::start(int check_status)
{
    static int in_start = 0;
    int result = 0;

    if (in_start == 0 && check_status && PQtransactionStatus(con) != PQTRANS_IDLE)
    {
        in_start = 1;
        msg.pwarning(W_START, "Transaktion ist beim Start nicht abgeschlossen");
        msg.line("letzter SQL-Befehl");
        msg.ignore_lang = 1;
        msg.line("%s", connections[con].in_transaction.c_str());
        result = 1;
    }
    else
    {
        connections[con].in_transaction = "";
    }

    in_start = 1;
    ignore_error = is_eof = ignore_oid = 0;

    connections[con].error_found = 0;
    connections[con].warning_found = 0;
    connections[con].ignore_message = 0;
    msg.clear();
    in_start = 0;

    return result;
}

void PgConnect::end()
{
    if (get_error() || get_warning() || msg.getErrorfound())
        rollback();
    else
        commit();

    start();
}

int PgConnect::int2ext_pgtype(int pgtype)
{
    switch (pgtype)
    {
    case BOOLOID:
        return DbConnect::BOOL;
    case INT2OID:
        return DbConnect::SHORT;
    case OIDOID:
        //case INF_SQL_CARDINAL:
        return DbConnect::LONG;
    case INT8OID:
    case INT4OID:
        return DbConnect::LONG;
    case FLOAT4OID:
        return DbConnect::FLOAT;
    case FLOAT8OID:
        return DbConnect::DOUBLE;
    case BPCHAROID:
        return DbConnect::CHAR;
    case CHAROID:
        return DbConnect::CHAR;
    case VARCHAROID:
        return DbConnect::CHAR;
    case TEXTOID:
        return DbConnect::CHAR;
    case TEXTARRAYOID:
        return DbConnect::CHAR;
    case TIMESTAMPOID:
    case TIMESTAMPTZOID:
        return DbConnect::CHAR;
    case NAMEOID:
        return DbConnect::CHAR;
    case INT2VECTOROID:
        return DbConnect::CHAR;
    case ACLITEMARRAYOID:
    case ACLITEMOID:
        return DbConnect::CHAR;
    case DATEOID:
        return DbConnect::CHAR;
    case TIMEOID:
        return DbConnect::CHAR;
    case UNKNOWNOID:
        return DbConnect::CHAR;
    case NUMERICOID:
        return DbConnect::DOUBLE;
    case XIDOID:
        //case INF_SQL_IDENTIFIER:
        //case INF_CHARACTER:
        return DbConnect::CHAR;
        //case CHARACTER_DATA:
    default:
    {
        std::map<long,long>::iterator i;
        if ( ( i = pgbasetypes.find(pgtype) ) != pgbasetypes.end() )
            return int2ext_pgtype(i->second);

        msg.pdebug(0, "Postgres Type <%d> ist nicht bekannt", pgtype);
        return -pgtype;
    }
    }
}

void PgConnect::setLang(std::string lang, int ready)
{
    return;
}

