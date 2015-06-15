#ifndef http_exec_mne
#define http_exec_mne

#include <map>
#include <string>

#include <message/message.h>
#include <embedweb/http_provider.h>

class HttpSysexec : public HttpProvider
{
    enum ErrorType
    {
        E_NOFUNC = 1,
        E_ERRORFOUND,

        E_MAX = 1000
    };

    enum WarningType
    {
        W_MAX = 1000
    };

    typedef void ( HttpSysexec::*SubProvider)(HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;

    SubProviderMap subprovider;

    Message msg;
    std::string font;

    void execute ( HttpHeader *h);

public:
    HttpSysexec( Http *h );
    virtual ~HttpSysexec();

    virtual std::string getPath() { return "sysexec"; }
    virtual int request (HttpHeader *h);

};

#endif /* http_exec_mne */
