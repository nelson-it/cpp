#ifndef http_translate_mne
#define http_translate_mne

class Http;
class HttpHeader;
 
#include <message/message.h>

class HttpTranslate
{
    Message msg;
public:
    HttpTranslate();
    ~HttpTranslate();

    void make_answer(HttpHeader *h, FILE *file);
};

#endif

