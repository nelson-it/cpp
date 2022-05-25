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
    std::string header;

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

    void read_header(const unsigned char *data, int length);
    void make_header();

public:

    DbWsHttp( ServerSocket *s, Ws *ws, DbHttpAnalyse *a, Database *db );
    virtual ~DbWsHttp();

    std::string getOpcode() { return "HTTP"; }
    virtual int request (WsAnalyse::Client *c );

    virtual const unsigned char *p_getData() { return (unsigned char *)act_h.content; }
    virtual int getDataLength() { return act_h.content_length; }

    virtual const unsigned char *p_getHeader() { return (unsigned char *)this->header.c_str(); }
    virtual int getHeaderLength() { return this->header.length(); }

    virtual void connect (int client) { this->analyse->connect(client); }
    virtual void disconnect (int client) { this->analyse->disconnect(client); }

};

#endif /* ws_http_mne */
