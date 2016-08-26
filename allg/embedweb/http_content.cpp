#ifdef PTHREAD
#include <pthread.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <sec_api/stdio_s.h>
#endif
#include <stdarg.h>

#include "http_content.h"

HttpContent::HttpContent()
{
}

HttpContent::~HttpContent()
{
}

void HttpContent::add_content(HttpHeader *h, const char *format, ... )
{
    va_list ap;
    va_start(ap, format);

    vfprintf(h->content, format, ap);
}


void HttpContent::add_contentb(HttpHeader *h, const char* buffer, int anzahl)
{
    fwrite(buffer, anzahl, 1, h->content );
}
