#ifndef http_mne
#define http_mne

#include <string>
#include <vector>

#include <message/message.h>
#include <ipc/s_provider.h>

#include "http_header.h"
#include "http_translate.h"

class HttpProvider;
class HttpMap;
class HttpAnalyse;

class Http : public Message::MessageClient
{
    friend class HttpSsi;

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

    typedef std::vector<HttpMap *> Maps;
    Maps maps;

    typedef std::map<std::string, HttpProvider *> Provider;
    Provider provider;

    std::map<int, std::string> meldungen;
    std::map<int, std::string> status_str;

    HttpHeader *act_h;

    HttpProvider *find_provider(Provider *p);

    virtual std::string get_meldungtext(int status);

    virtual void make_answer();
    virtual void write_header();
    virtual void write_trailer();

    virtual void send();

    virtual void mk_error(const char *typ, char *str);
public:

    Http( ServerSocket *s, HttpAnalyse *a, int register_thread = 1);
    virtual ~Http();

    virtual void init_thread() {};

    virtual void get(HttpHeader *h);
    HttpAnalyse *p_getHttpAnalyse() { return analyse; }

    std::string mkmap(std::string filename);

    virtual void disconnect( int client );

    void add_provider(HttpProvider *);
    void del_provider(HttpProvider *);

    ServerSocket *getServersocket() { return this->s; }

    void perror( char *str);
    void pwarning( char *str);
    void pmessage( char *str);
    void pline(char *str);

};

#endif /* http_mne */
