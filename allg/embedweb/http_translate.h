#ifndef http_translate_mne
#define http_translate_mne

class Http;
class HttpHeader;
 
#include <message/message.h>
#include "http_content.h"

class HttpTranslate : private HttpContent
{
    Message msg;
public:
    HttpTranslate();
    ~HttpTranslate();

    void make_answer(HttpHeader *h, FILE *file);
};

#endif

