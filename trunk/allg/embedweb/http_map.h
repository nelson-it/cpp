#ifndef http_map_mne
#define http_map_mne

#include <map>
#include <string>

#include <utils/cslist.h>
#include <message/message.h>

#include "http_provider.h"

class HttpMap : public HttpProvider
{
    enum ErrorType
    {
        E_OK = 0
    };

    Message msg;

    std::string origpath;
    std::string path;
    CsList files;

public:
    HttpMap(Http *http, std::string path, std::string origpath,
                        std::string files);
    virtual ~HttpMap();

    virtual std::string getPath() { return path; }
    virtual int request (HttpHeader *h);

    std::string mkfilename(HttpHeader *h, std::string filename); 

};

#endif /* http_map_mne */
