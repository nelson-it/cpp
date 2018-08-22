#ifndef http_mne
#define http_mne

#include <string>
#include <vector>

#include <message/message.h>
#include <ipc/s_provider.h>
#include <argument/argument.h>

#include "http_header.h"
#include "http_translate.h"
#include "http_content.h"

class HttpProvider;
class HttpAnalyse;

class Http : protected HttpContent, public Message::MessageClient
{
    // Membervariablen
    // ===============
    int no_cache;

protected:
    Message msg;
    HttpTranslate http_translate;

    ServerSocket *s;
    HttpAnalyse *analyse;

    enum ERROR_TYPE
    {
	E_OK,
	E_PRO_EXISTS,
	E_PRO_NOT_EXISTS,
	E_TEMPFILE,
	E_EXEC,
	E_WRITEHEADER,
	E_NOTFOUND,
	E_STATUS,
	E_CONTENTLENGTH,

	MAX_ERROR = 100
    };

    enum WARNING_TYPE
    {
	W_CONTENTLENGTH = 1
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

    typedef std::map<std::string, HttpProvider *> Provider;
    Provider provider;

    std::map<int, std::string> meldungen;
    std::map<int, std::string> status_str;

    HttpHeader *act_h;

    HttpProvider *find_provider(Provider *p);

    virtual void make_meldung();
    virtual void make_answer();
    virtual void make_error();
    virtual void make_translate();

    virtual void write_header();
    virtual void write_content() {  s->write(act_h->client, act_h->content, act_h->content_length); };
    virtual void write_trailer();

    virtual void mk_error(const char *typ, char *str);

public:

    Http( ServerSocket *s, HttpAnalyse *a );
    virtual ~Http();
    virtual void init_thread() {};

    virtual void get(HttpHeader *h);
    virtual void make_content(HttpHeader *h = NULL);

    virtual void disconnect( int client );

    void add_provider(HttpProvider *);
    void del_provider(HttpProvider *);

    void perror( char *str);
    void pwarning( char *str);
    void pmessage( char *str);
    void pline(char *str);

    virtual int  check_group(HttpHeader *h, const char *group) { return false; }
    virtual int  check_sysaccess(HttpHeader *h )               { return false; }
    virtual int  check_ip(const char *ip);

};

#endif /* http_mne */
