#ifndef http_content_mne
#define http_content_mne

#include <stdio.h>

#include "message/message.h"
#include "http_header.h"

class HttpContent
{
    Message msg_content;

    enum ErrorType
    {
        E_FILE = 1,

        E_MAX
    };
public:
    HttpContent();
    virtual ~HttpContent();

    virtual void del_content(HttpHeader *h );

    virtual void add_content(HttpHeader *h, const char *format, ... );
    virtual void add_contentb(HttpHeader *h, const char *buffer, int anzahl );

    virtual void contentf(HttpHeader *h, FILE *fp );
    virtual void contentf(HttpHeader *h, const char *file );

    virtual void save_content(HttpHeader *h, const char *file);
};

#endif /* http_content_mne */
