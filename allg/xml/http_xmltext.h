#ifndef http_xmltext_mne
#define http_xmltext_mne

#include <map>
#include <string>

#include <message/message.h>

#include <embedweb/http_provider.h>

class HttpXmlText : public HttpProvider
{
    Message msg;
    enum ErrorType
    {
        E_OK = 0
    };

    typedef void ( HttpXmlText::*SubProvider)(HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;

    SubProviderMap subprovider;

    void html ( HttpHeader *h);
    void htmledit ( HttpHeader *h);

public:
    HttpXmlText( Http *h );
    virtual ~HttpXmlText();

    virtual std::string getPath() { return "xmltext"; }
    virtual int request (HttpHeader *h);

};

#endif /* http_xmltext_mne */
