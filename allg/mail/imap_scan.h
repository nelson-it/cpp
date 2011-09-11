#ifndef imap_scan_mne
#define imap_scan_mne

#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <string>
#include <vector>

#include <message/message.h>
#include <db/database.h>

#include "imap.h"

class ImapScan
{
    enum ErrorTypes
    {
        E_OK = 0,
        E_MAILBOX
    };

    enum WarningTypes
    {
        W_SCAN = 1
    };

#ifdef PTHREAD
    static pthread_mutex_t mutex;
    static int mutex_init;
#endif

    Message msg;
    Database *db;

    Imap::Headers headers;

    class Mailbox
    {
    public:
        std::string server;
        std::string login;
        std::string passwd;

        Mailbox ()
        {
        }
        Mailbox ( std::string server, std::string login, std::string passwd )
        {
            this->server = server;
            this->login = login;
            this->passwd = passwd;
        }
    };

public:
    ImapScan(Database *db);
    ~ImapScan();

    void scan( std::string mailboxid = "", int full = 0);

    Imap::Headers getHeader() { return headers;  }
    void clearHeader()        { headers.clear(); }
};

#ifdef PTHREAD
class ImapScanThread
{
public:
    pthread_t id;
    int abort;
    ImapScan imapscan;

    ImapScanThread(Database *db);
    ~ImapScanThread();
};
#endif

#endif /* imap_scan_mne */
