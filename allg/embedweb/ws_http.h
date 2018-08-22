#ifndef ws_http_mne
#define ws_http_mne

#include <string>
#include <vector>

#include <message/message.h>

#include "http_request.h"
#include "http.h"

#include "ws_header.h"
#include "ws_analyse.h"
#include "ws_provider.h"

class WsHttp : private HttpRequest, public Http, public WsProvider
{

protected:
    Message msg;

    HttpHeader act_h;

    enum ERROR_TYPE
    {
        E_PRO_EXISTS = 1,
        E_PRO_NOT_EXISTS,
        MAX_ERROR = 100
    };

    enum WARNING_TYPE
    {
        MAX_WARNING = 100
    };

    enum DEBUG_TYPE
    {
    };

    void analyse_header(const unsigned char *data, int length);

public:

    WsHttp( ServerSocket *s, Ws *ws );
    virtual ~WsHttp();

    std::string getOpcode() { return "HTTP"; }
    virtual int request (WsAnalyse::Client *c );
    virtual unsigned char *p_getData() { return (unsigned char *)act_h.content; }
    virtual int getLength() { return act_h.content_length; }

};

#endif /* ws_http_mne */
