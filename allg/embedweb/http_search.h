#ifndef http_search_mne
#define http_search_mne

#include <map>
#include <string>

#include <message/message.h>

#include "http_provider.h"

class HttpSearchPath : public HttpProvider
{
    enum ErrorType
    {
        E_FILE_FOUND = 1,
        E_FILE_OPEN
    };

    Message msg;
	std::string path;
    std::vector<std::string> search;
    
public:
    HttpSearchPath( Http *h, std::string path, int noadd = 0 );
    virtual ~HttpSearchPath();

    virtual std::string getPath() { return path; }
    virtual int request (HttpHeader *h);

};

#endif /* http_search_mne */
