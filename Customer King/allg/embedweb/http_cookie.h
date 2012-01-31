#ifndef http_cookie_mne
#define http_cookie_mne

#include <string>
#include <map>

class HttpCookie 
{
    typedef std::map<std::string, std::string> Cookies;
    Cookies cookies;

public:
    HttpCookie() {};
    virtual ~HttpCookie() {};

    void setCookie( std::string cookie);
    void addCookie( std::string cookie, std::string value )
    {
        this->cookies[cookie] = value;
    }
    void clear() { cookies.clear(); };

    std::string operator [] ( const char *name);

};

#endif /* http_mne */
