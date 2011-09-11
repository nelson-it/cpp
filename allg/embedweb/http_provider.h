#ifndef http_provider_mne
#define http_provider_mne

#include <string>

#include "http.h"

class HttpProvider
{
protected:

    Http *http;
    HttpProvider()
	{ this->http = NULL; }
    
public:
     HttpProvider( Http *http )
	 { this->http = http; }
     virtual ~HttpProvider()
	 { if ( http != NULL ) http->del_provider(this); }

     virtual std::string getPath() = 0;
     virtual int request (HttpHeader *h) = 0;

};

#endif /* http_provider_mne */
