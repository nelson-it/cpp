#ifndef http_analyse_mne
#define http_analyse_mne

#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <string>
#include <vector>
#include <deque>

#include <message/message.h>
#include <ipc/s_provider.h>

#include "http_header.h"

class Http;

class HttpAnalyse : public SocketProvider
{
#ifdef PTHREAD
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

        HttpAnalyseThreadParam(Http *http);
	~HttpAnalyseThreadParam();
    };

#endif

private:

    typedef std::map<std::string, std::string> ContentTypes;

    typedef std::vector<std::string> Header;
    typedef std::map<int, Header> Headers;

    typedef std::map<int, HttpHeader*> HttpHeaders;

    #ifdef PTHREAD
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
    #else
    typedef std::vector<Http *> Https;
    #endif

    // Membervariablen
    // ===============
    HttpHeader *act_h;

protected:
    Message msg;

    std::vector<std::string>  serverpath;
    ContentTypes content_types;

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
	D_SEND = 2,
	D_CON  = 3,
        D_HEADER = 4,
	D_RAWHEADER = 4,
        D_HTTP = 5,
        D_EXEC = 5
    };

    virtual void analyse_header();
    virtual void read_postvalues();
    virtual void check_user(HttpHeader *h) {};

public:
    HttpAnalyse( ServerSocket *s);
    virtual ~HttpAnalyse();

    std::string getProvidername() { return "Http";  }
    std::vector<std::string> getServerpath() { return serverpath; }
    void request( int client, char *buffer, long size );

    virtual void analyse_requestline(std::string request, HttpHeader *h);

    virtual void disconnect( int client );

    void add_http( Http *http);
    void del_http( Http *http);


};

#endif /* http_analyse_mne */
