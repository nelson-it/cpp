#ifndef http_request_mne
#define http_request_mne

#include <message/message.h>
#include <ipc/s_provider.h>

#include "http_header.h"

class HttpRequest
{
    Message msg;

protected:
    ServerSocket *serversocket;

    typedef std::map<std::string, std::string> ContentTypes;

    std::vector<std::string>  serverpath;
    std::map<std::string,std::string>  datapath;
    std::string dataroot;

    ContentTypes content_types;


    enum ERROR_TYPE
    {
        E_CLIENT_UNKOWN,
        E_REQUEST,
        E_REQUEST_TO_SHORT,
        E_AUTH_NOT_SUPPORTED,

        MAX_ERROR = 100
    };

    enum WARNING_TYPE
    {
    };

    enum DEBUG_TYPE
    {
        D_HEADER = 4,
        D_RAWHEADER = 4,
    };

    typedef std::vector<std::string> Header;

    virtual void analyse_header(Header *h, HttpHeader *act_h);

    virtual void analyse_requestline(std::string arg, HttpHeader *h);
    virtual void analyse_hostline(std::string arg, HttpHeader *h);
    virtual void analyse_authline(std::string arg, HttpHeader *h);

public:
    HttpRequest( ServerSocket *s);
    virtual ~HttpRequest();
};

#endif /* http_request_mne */
