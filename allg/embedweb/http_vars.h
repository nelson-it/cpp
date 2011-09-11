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
    int start_error;

    Vars vars;
    Files files;

public:
    HttpVars() : msg("HttpVars")
    {}

    virtual ~HttpVars();

    void setVars( std::string vars);
    void setMultipart( std::string boundary, char *data);

    Vars *p_getVars() { return &vars; }

    void clear();
    void clear( const char *name);

    std::string operator [] ( const char *name);
    std::string operator [] ( std::string s) { return operator[](s.c_str()); }

    int exists( const char *name);
    int exists( std::string name) { return exists(name.c_str()); }

    std::string data(const char *name);
    std::string data(std::string s) { return data(s.c_str()); }

    std::string url_decode(std::string val);

};

#endif /* http_mne */
