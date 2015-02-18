#ifndef http_provider_mne
#define http_provider_mne

#include <string>

#include "http.h"

#if defined(__MINGW32__) || defined(__CYGWIN__)
int inet_aton(const char *cp_arg, struct in_addr *addr);
#endif

class HttpProvider
{
    enum E_ERROR
    {
        E_PERM = 1
    };
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

     virtual int check_ip(const char *ip, int host);
};

#endif /* http_provider_mne */
