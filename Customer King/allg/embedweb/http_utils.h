#ifndef http_utils_mne
#define http_utils_mne

#include <map>
#include <string>

#include <message/message.h>

#include "http_provider.h"

class HttpUtils : public HttpProvider
{
    enum ErrorType
    {
        E_GDSTRING = 1
    };

    enum WarningType
    {
        W_LOGOUT = 1
    };

    typedef void ( HttpUtils::*SubProvider)(HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;

    SubProviderMap subprovider;

    Message msg;
    std::string font;

    void mk_window(HttpHeader *h, char *str);
    void count       ( HttpHeader *h);
    void uhr         ( HttpHeader *h);
    void time        ( HttpHeader *h);
    void logout      ( HttpHeader *h);
    void file        ( HttpHeader *h);
    void locale_xml  ( HttpHeader *h);
public:
    HttpUtils( Http *h );
    virtual ~HttpUtils();

    virtual std::string getPath() { return "utils"; }
    virtual int request (HttpHeader *h);

};

#endif /* http_utils_mne */
