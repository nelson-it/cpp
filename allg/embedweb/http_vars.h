#ifndef http_vars_mne
#define http_vars_mne

#include <string>
#include <map>

#include <message/message.h>

class HttpVars;
class HttpVarsMultipart
{
    HttpVars *vars;

    char *buf;
    int bufpos;
    int buflen;
    int bufsize;

    int waitfile;

    std::string boundary;
    std::string name;
    std::string value;
    std::string str;
    std::string content_type;

    int startfound;
    int endfound;
    int isvalue;

    FILE *wd;
    char filename[512];

    FILE *opentmp(char *filename);

    void putfile();
    void putline();

public:
    HttpVarsMultipart(HttpVars *vars, std::string boundary);
    ~HttpVarsMultipart();

    void put(char *buffer, int size);
    int isready () { return endfound; }
};

class HttpVars 
{
    friend class HttpVarsMultipart;

public:
    typedef std::map<std::string, std::string> Vars;
    typedef std::map<std::string, std::string> Files;

private:
    enum ErrorTypes
    {
        E_FILEOPEN = 1,
        E_DECODE,
        E_REQUEST
    };

    Message msg;

    Vars vars;
    Files files;

    Vars extravars;

    HttpVarsMultipart *multipart;

public:
    HttpVars() : msg("HttpVars")
    { multipart = NULL; }

    virtual ~HttpVars();

    void setVars( std::string vars);
    void setMultipart( std::string boundary, char *values, int size);
    void setVar(std::string id, std::string value)
    {
        vars[id] = value;
    }

    Vars *p_getVars() { return &vars; }
    Vars *p_getExtraVars() { return &extravars; }

    void clear();
    void clear( const char *name);

    std::string operator [] ( const char *name);
    std::string operator [] ( std::string s) { return operator[](s.c_str()); }

    int exists( const char *name);
    int exists( std::string name) { return exists(name.c_str()); }

    std::string getFile(const char* name);

    std::string data(const char *name, int raw = 0 );
    std::string data(std::string s , int raw = 0) { return data(s.c_str(), raw); }

    std::string url_decode(std::string val);

};

#endif /* http_mne */
