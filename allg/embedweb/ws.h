#ifndef ws_mne
#define ws_mne

#include <string>
#include <vector>

#include <message/message.h>
#include <ipc/s_provider.h>

#include "http_request.h"
#include "http.h"

#include "ws_header.h"
#include "ws_analyse.h"

class WsProvider;
class Ws : protected HttpContent
{

protected:
    Message msg;

    ServerSocket *s;
    WsAnalyse *analyse;

    std::string act_opcode;
    WsAnalyse::Client *act_c;

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

    typedef std::map<std::string, WsProvider *> Provider;
    Provider provider;

    void read_opcode();
    void send_wsheader(int size);
public:

    Ws( ServerSocket *s, WsAnalyse *a, int register_thread = 1 );
    virtual ~Ws();

    virtual void init_thread() {};

    virtual void get(WsAnalyse::Client *c);

    virtual void disconnect( int client );
    virtual void unlock_client() {};

    WsProvider* find_provider(std::string opcode);
    void add_provider(WsProvider *);
    void del_provider(WsProvider *);
};

#endif /* ws_mne */
