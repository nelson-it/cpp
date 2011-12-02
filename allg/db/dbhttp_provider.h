#ifndef dbhttp_provider_mne
#define dbhttp_provider_mne

#include <string>

#include <embedweb/http_provider.h>
#include <db/database.h>

#include "dbhttp.h"

class DbHttpProvider : public HttpProvider
{
protected:

    DbHttp *http;
public:
     DbHttpProvider( DbHttp *http )
	 { this->http = http; }
     virtual ~DbHttpProvider()
	 { http->del_provider(this); }

     virtual std::string getPath() = 0;

     virtual int check_request(Database *db, HttpHeader *h)
     {
         return 0;
     }
     virtual int request (Database *db, HttpHeader *h, int nologin )
     {
         return 0;
     }
     virtual int request (Database *db, HttpHeader *h ) = 0;
     virtual int request (HttpHeader *h)
         { exit(999); }

};

#endif /* dbhttp_provider_mne */
