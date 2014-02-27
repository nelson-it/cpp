#ifndef http_filesystem_mne
#define http_filesystem_mne

#include <map>
#include <string>

#include <message/message.h>

#include "http_provider.h"

class HttpFilesystem : public HttpProvider
{
    typedef void ( HttpFilesystem::*SubProvider)(HttpHeader *h);
    typedef std::map<std::string, SubProvider> SubProviderMap;

    SubProviderMap subprovider;

    Message msg;

protected:
    enum ErrorType
    {
        E_FILENOTFOUND = 1,
        E_NEEDNAME,

        E_CREATEFILE,
        E_DELFILE,

        E_MAX = 1000
    };

    enum WarningType
    {
        W_MAX = 1000
    };

    virtual std::string getRoot(HttpHeader *h);
    virtual std::string getDir(HttpHeader *h);

    virtual std::string check_path(std::string dir, std::string name, int needname = 1, int errormsg = 1 );
    virtual std::string check_path(HttpHeader *h, std::string name, int needname = 1 , int errormsg = 1);

    std::string root;
    std::string dir;

    std::string path;

    std::set<std::string> dirs;
    std::set<std::string> files;

    virtual void readdir(HttpHeader *h);

    void ls     ( HttpHeader *h);
    void mkdir  ( HttpHeader *h);
    void rmdir  ( HttpHeader *h);

    void mkfile ( HttpHeader *h);
    void rmfile ( HttpHeader *h);

    void mv     ( HttpHeader *h);

public:
    HttpFilesystem( Http *h, int noadd = 0 );
    virtual ~HttpFilesystem();

    virtual std::string getPath() { return "file"; }
    virtual int request (HttpHeader *h);

};

#endif /* http_filesystem_mne */
