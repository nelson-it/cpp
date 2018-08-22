#ifndef dbws_http_mne
#define dbws_http_mne

#include <string>
#include <vector>

#include <message/message.h>

#include "dbhttp.h"

//#include "ws_header.h"
//#include "ws_analyse.h"
#include <embedweb/ws_provider.h>

class DbWsHttp : private HttpRequest, public DbHttp, public WsProvider
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

    DbWsHttp( ServerSocket *s, Ws *ws, DbHttpAnalyse *a, Database *db );
    virtual ~DbWsHttp();

    std::string getOpcode() { return "HTTP"; }
    virtual int request (WsAnalyse::Client *c );
    virtual unsigned char *p_getData() { return (unsigned char *)act_h.content; }
    virtual int getLength() { return act_h.content_length; }

};

#endif /* ws_http_mne */
