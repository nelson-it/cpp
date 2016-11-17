#ifdef PTHREAD
#include <pthread.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <sec_api/stdio_s.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "http_content.h"

HttpContent::HttpContent() :
        msg_content("HttpContent")
{
}

HttpContent::~HttpContent()
{
}

void HttpContent::del_content(HttpHeader *h)
{
    h->content[0] = '\0';
    h->content_length = 0;
}
void HttpContent::add_content(HttpHeader *h, const char *format, ...)
{
    va_list ap;
    int n;

    va_start(ap, format);
    n = vsnprintf(&h->content[h->content_length], h->content_maxsize - h->content_length, format, ap);
    va_end(ap);

    while ((h->content_length + n + 1) >= h->content_maxsize)
    {
        h->content_maxsize += h->CONTENT_SIZE;
        char *str = new char[h->content_maxsize];
        memcpy(str, h->content, h->content_length);
        delete[] h->content;
        h->content = str;

        va_start(ap, format);
        n = vsnprintf(&h->content[h->content_length], h->content_maxsize - h->content_length, format, ap);
        va_end(ap);
    }

    h->content_length += n;
}

void HttpContent::add_contentb(HttpHeader *h, const char* buffer, int anzahl)
{
    if (h->content_length + anzahl > h->content_maxsize)
    {
        while ((h->content_length + anzahl) >= h->content_maxsize)
            h->content_maxsize += h->CONTENT_SIZE;
        char *str = new char[h->content_maxsize];
        memcpy(str, h->content, h->content_length);
        delete[] h->content;
        h->content = str;
    }

    memcpy(&h->content[h->content_length], buffer, anzahl);
    h->content_length += anzahl;
}

void HttpContent::contentf(HttpHeader *h, FILE *fp)
{
    int size;
    char *buffer;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 0) size = 0;

    buffer = new char[size];
    fread(buffer, size, 1, fp);
    h->content_length = 0;
    add_contentb(h, buffer, size);
    delete[] buffer;
}

void HttpContent::contentf(HttpHeader *h, const char* file)
{
    FILE *f;
    if ((f = fopen(file, "rb")) == NULL)
    {
        msg_content.perror(E_FILE, "konnte Datei <%s> nicht öffnen", file);
        h->content_length = 0;
        return;
    }

    contentf(h, f);
    fclose(f);
}

void HttpContent::save_content(HttpHeader *h, const char* file)
{
    FILE *f;
    if ((f = fopen(file, "wb")) != NULL )
        fwrite(h->content, h->content_length, 1, f);
    else msg_content.perror(E_FILE, "konnte Datei <%s> nicht öffnen", file);

    fclose(f);
}
