#ifndef dbconnect_mne
#define dbconnect_mne

#include <iconv.h>

#include <string>
#include <vector>

#include <message/message.h>

class DbConnect
{
public:
    enum ERROR_TYPE
    {
        E_OK = 0,
        E_NORESULT,

        E_MAXERROR  = 100

    };

    enum WARNING_TYPE
    {
        W_STRINGDOUBLE = 1
    };

    enum READYTYPE
    {
        NOT_READY = 0,
        READY
    };

    enum VARTYPE
    {
        UNKNOWN = -1,

        BOOL = 1,
        CHAR,
        SHORT,
        LONG,
        FLOAT,
        DOUBLE,

        BINARY =100,

        DATETIME = 1000,
        DATE,
        TIME,
        INTERVAL,
        DAY,
        QUARTER,
        CDATETIME,
        CDATE,
        CTIME,

        EMAIL = 1010,
        LINK,

        TYPECOL = 1100,

        MAX_VARTYPE = 100000
    };

    class Result
    {
    public:
        int typ;
        void *value;
        void *rbuff;
        int length;
        int isnull;

        Result();
        Result(const Result &in);
        Result &operator=(const Result &in);
        virtual ~Result();

        bool operator==  (const Result &in) const;
        bool operator!=  (const Result &in) const;
        bool operator<   (const Result &in) const;
        bool operator>   (const Result &in) const;
        bool operator<=  (const Result &in) const;
        bool operator>=  (const Result &in) const;

        operator char *() { return (char *)value; }
        operator double() { return *(double *) value; }
        operator long()   { return *(long *) value; }

        char *format( Message *msg = NULL,
                char *str = NULL, int length = 0,
                const char *format = NULL);
    };

    typedef std::vector<Result> ResultVec;
    typedef std::vector<ResultVec> ResultMat;

protected:
    Message msg;
    ResultMat result;
    iconv_t iv;

public:
    DbConnect() : msg("DbConnect") {iv = 0;};
    virtual ~DbConnect() {};

    virtual void mk_string( std::string &str, int nodelimiter = 0);

    virtual std::string getValue( int typ, std::string str);

    virtual std::string getUser() = 0;
    virtual int have_connection() = 0;
    virtual int eof() = 0;

    virtual void commit() = 0;
    virtual void rollback() = 0;

    virtual int  start(int check_status = 1) = 0;
    virtual void end() = 0;

    virtual int add_schema( std::string schema, int ready = 0) = 0;
    virtual int del_schema( std::string schema, int ready = 0) = 0;

    virtual std::vector<std::string> getSchemas() = 0;

    virtual int setCurschema( std::string schema, int ready = 0) = 0;
    virtual std::string getCurschema(int ready = 0) = 0;

    virtual int execute(const char *stm, int ready = 0,
            int no_clear_result = 0) = 0;

    int execute( std::string stm, int ready = 0, int no_clearresult = 0 )
    {
        return execute(stm.c_str(), ready, no_clearresult);
    }
    virtual int get_error() = 0;
    virtual int get_warning() = 0;

    virtual void setIgnore_error() = 0;
    virtual void resetIgnore_error() = 0;

    virtual void setLang(std::string, int ready = 0) = 0;

    int have_result() { return (result.size() > 0); }
    ResultVec    get_first_result()
    {
        if ( have_result() )
            return result[0];
        else
        {
            ResultVec v;
            msg.perror(E_NORESULT, "Keine Ergebniszeile verfügbar");
            return v;
        }
    }

    ResultVec *p_get_first_result()
    {
        if ( have_result() )
            return &result[0];
        else
        {
            msg.perror(E_NORESULT, "Keine Ergebniszeile verfügbar");
            return NULL;
        }
    }

    ResultMat    get_result() { return result;  }
    ResultMat *p_get_result() { return &result; }

    ResultMat    getResult() { return result;  }
    ResultMat *p_getResult() { return &result; }

    virtual std::string mk_index();
};


#endif /* dbconnect_mne */
