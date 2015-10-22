#ifndef http_vars_mne
#define http_vars_mne

#include <string>
#include <map>

#include <message/message.h>

class HttpVars 
{
public:
    typedef std::map<std::string, std::string> Vars;
    typedef std::map<std::string, std::string> Files;

private:
    enum ErrorTypes
    {
        FILEOPEN = 1
    };

    Message msg;

    Vars vars;
    Files files;

    Vars extravars;
public:
    HttpVars() : msg("HttpVars")
    {}

    virtual ~HttpVars();

    void setVars( std::string vars);
    void setMultipart( std::string boundary, char *data);

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
