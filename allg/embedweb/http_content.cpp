#ifdef PTHREAD
#include <pthread.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <sec_api/stdio_s.h>
#endif
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "http_content.h"

HttpContent::HttpContent()
{
}

HttpContent::~HttpContent()
{
}

void HttpContent::add_content(HttpHeader *h, const char *format, ....)
{
    va_list ap;
    va_start(ap, format);

    vfprintf(h->content, format, ap);
}


void HttpContent::add_content(HttpHeader *h, buffer, anzahl)
{
    fwrite(buffer, anzahl, 1, h->content);
}
