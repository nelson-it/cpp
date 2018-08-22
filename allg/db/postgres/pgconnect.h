#ifndef pgconnect_mne
#define pgconnect_mne

#include <map>
#include <libpq-fe.h>

#include <message/message.h>

#include "../dbconnect.h"

class PgConnect : public DbConnect
{
    class Connections
    {
        friend class PgConnect;

        int anzahl_connect;
        int extension;
        int ignore_message;
        int error_found;
        int warning_found;
        std::string appl_schema;
        std::string in_transaction;
        pthread_mutex_t mutex;

    public:
        Connections()
        {
            anzahl_connect = 0;
            extension = 0;
            ignore_message = 0;
            error_found = 0;
            warning_found = 0;

            pthread_mutex_init(&mutex, NULL);
        }

    };
    static std::map<PGconn*, Connections> connections;
    static PGconn* last_con;

    PGconn *con;

    int extension;

    char *errstr;
    char *p_errstr;
    char *errresult;
    std::string errlangstr;

    void open_connection(const char *dbname, const char *user, const char *passwd);
    void close_connection();

    void mk_result(PGresult *res, const char *stm);
    char *mk_error(PGresult *res = NULL);

protected:


    enum ERROR_TYPE
    {
	E_CONNECTION_NOT_OK = DbConnect::E_MAXERROR,
	E_CONNECTION_NULL,

	E_TYPE_UNKNOWN,

	E_UNKNOWN_SCHEMA,

	E_EXECUTE_CON_NULL,
	E_EXECUTE_ERROR,
	E_EXECUTE_WARNING,
	E_EXECUTE_MESSAGE,
	E_EXECUTE_STATUS,
	E_EXECUTE_NORESULT,

	E_ERROR_MAX = 1000
    };

    enum WARNING_TYPE
    {
        W_START = 1,

        W_WARNING_MAX = 1000
    };

    enum DEBUG_TYPE
    {
	D_CON = 1,
	D_STM = 6,
	D_CONCOUNT = 8
    };

    static std::map<long, long> pgbasetypes;

    int is_eof;
    int ignore_error;
    int ignore_oid;

public:

    PgConnect(std::string dbname, std::string user="", std::string passwd="");
    PgConnect(const PgConnect *con);
    PgConnect();
    ~PgConnect();

    void setApplschema ( std::string appl_schema ) { if ( con != NULL ) this->connections[con].appl_schema = appl_schema; };
    std::string getApplschema () { return (con != NULL ) ? this->connections[con].appl_schema : ""; };

    std::string getValue(int typ, std::string str);

    std::string getUser()
        { if ( con != NULL ) return PQuser(con); else return "";}
    int have_connection() { return con != NULL; }

    void notice( const char *message);
    int start(int check_status = 1);
    void end();

    int have_extension() { return extension > 0; }

    int add_schema(std::string schema, int ready = 0);
    int del_schema(std::string schema, int ready = 0);

    std::vector<std::string> getSchemas();

    int setCurschema(std::string schema, int ready = 0);
    std::string getCurschema(int ready = 0);

    int execute(const char *stm, int ready = 0, int no_clear_result = 0);

    void commit();
    void rollback();

    int int2ext_pgtype( int pgtype);

    int get_error() { return connections[con].error_found; }
    int get_warning() { return connections[con].warning_found; }

    int eof() { return is_eof; }

    void   setIgnore_error() { ignore_error = 1; }
    void resetIgnore_error() { ignore_error = 0; }

    void setLang(std::string lang, int ready = 0);
};

#define I_MES(x) { connections[con].ignore_message = 1; x; connections[con].ignore_message = 0; };
#define I_ERR(x) { ignore_error = 1; x; ignore_error = 0; };
#define I_OID(x) { ignore_oid = 1; x; ignore_oid = 0; };

#endif /* pgconnect_mne */

