#ifndef dbhttp_utils_imap_mne
#define dbhttp_utils_imap_mne

#include <map>
#include <string>

#include <embedweb/http_provider.h>
#include "dbhttp_provider.h"

class DbHttpUtilsImap : public DbHttpProvider
{
    Message msg;

    enum ERROR_TYPE
    {

        E_TYPE = 100,
        E_MAILBOX

    };

    typedef void (DbHttpUtilsImap::*SubProvider)(Database *db,HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;
    SubProviderMap subprovider;

    void folder_xml     (Database *db, HttpHeader *h);
    void rescan_xml     (Database *db, HttpHeader *h);

    void folder_json    (Database *db, HttpHeader *h);
    void rescan_json    (Database *db, HttpHeader *h);

public:
    DbHttpUtilsImap( DbHttp *h );
    virtual ~DbHttpUtilsImap();

    virtual std::string getPath() { return "db/utils/imap"; }
    virtual int request (Database *db, HttpHeader *h);

};

#endif /* dbhttp_utils_imap_mne */
