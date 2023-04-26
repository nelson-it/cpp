#ifndef http_analyse_mne
#define http_analyse_mne

#include <pthread.h>
#include <string>
#include <vector>
#include <deque>
#include <map>

#include <message/message.h>
#include <ipc/s_provider.h>

#include "http_request.h"

class Http;

class HttpAnalyse : public HttpRequest, public SocketProvider
{
public:
    class HttpAnalyseThreadParam
    {
    public:
        pthread_t id;
        pthread_mutex_t mutex;

        Http *http;
        HttpAnalyse *analyse;

        HttpHeader *act_h;
        int abort;

        HttpAnalyseThreadParam(Http *http, HttpAnalyse *analyse);
        ~HttpAnalyseThreadParam();

        void disconnect(int client);
    };

private:

    typedef std::map<int, Header> Headers;
    typedef std::map<int, HttpHeader*> HttpHeaders;

    friend class HttpAnalyseThreadParam;
    typedef std::vector<HttpAnalyseThreadParam *> Https;
    typedef std::deque<HttpHeader *> HttpWaitingHeaders;

protected:

    pthread_mutex_t header_mutex;
    pthread_cond_t  header_cond;

    virtual void        putHeader(HttpHeader *h);
public:
    virtual HttpHeader *getHeader();
    virtual void        releaseHeader(HttpHeader *h);


private:

    // Membervariablen
    // ===============
    HttpHeader *act_h;

protected:
    Message msg;

    Headers headers;
    HttpHeaders http_headers;
    HttpWaitingHeaders waiting_headers;

    Https   https;

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
        W_HTTP = 1,
        MAX_WARNING = 100
    };

    enum DEBUG_TYPE
    {
        D_CON  = 1,
        D_SEND = 2,
        D_CLIENT = 3,
        D_HEADER = 4,
        D_RAWHEADER = 4,
        D_HTTP = 5,
        D_EXEC = 5,
        D_POSTDATA
    };

    virtual void read_header();
    virtual int  check_group(HttpHeader *h, const char *group) { return false; }

public:
    HttpAnalyse( ServerSocket *s);
    virtual ~HttpAnalyse();

    std::string getProvidername() { return "Http";  }
    std::vector<std::string> getServerpath() { return serverpath; }
    void request( int client, char *buffer, long size );

    virtual void connect( int client );
    virtual void disconnect( int client );

    void add_http( Http *http);
    void del_http( Http *http);


};

#endif /* http_analyse_mne */
