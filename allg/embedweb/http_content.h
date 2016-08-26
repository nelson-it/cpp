#ifndef http_content_mne
#define http_content_mne

#include <string>
#include <vector>

#include <message/message.h>
#include <ipc/s_provider.h>

#include "http_header.h"

class HttpProvider;
class HttpMap;
class HttpAnalyse;

class HttpContent
{
public:
    HttpContent();
    virtual ~HttpContent();

    virtual void add_content(HttpHeader *h, const char *format, ...);
    virtual void add_contentb(HttpHeader *h, const char *buffer, int anzahl);
};

#endif /* http_content_mne */
