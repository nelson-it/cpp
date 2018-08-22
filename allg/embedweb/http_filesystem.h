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
        E_CREATELINK,

        E_FILEEXISTS,
        E_FILE_OPEN,

        E_MAX = 1000
    };

    enum WarningType
    {
        W_MAX = 1000
    };

    enum DebugType
    {
        D_ROOTDIRS = 1
    };

    virtual std::string getRoot(HttpHeader *h);
    virtual std::string getDir(HttpHeader *h);

    virtual std::string check_path(std::string dir, std::string name, int needname = 1, int errormsg = 1, std::string *result = NULL );
    virtual std::string check_path(HttpHeader *h, std::string name, int needname = 1 , int errormsg = 1 , std::string *result = NULL );

    virtual int findfile(HttpHeader *h);

    std::string dataroot;
    std::string root;
    std::string dir;

    std::string cacheroot;

    std::string path;
    struct stat statbuf;

    struct FileData
    {
        std::string name;
        struct stat statbuf;
    };

    enum FileDataSort
    {
        FD_NAME = 0,
        FD_CREATE,
        FD_MOD,
        FD_ACCESS
    };

    std::vector<FileData> dirs;
    std::vector<FileData> files;

    FileDataSort qs_type;

    virtual int  quicksort_check(FileData *data1, FileData *data2);
    virtual void quicksort(std::vector<FileData> &sort, int left, int right);

    virtual void readdir(HttpHeader *h);

    void ls     ( HttpHeader *h);
    void mkdir  ( HttpHeader *h);
    void rmdir  ( HttpHeader *h);

    void mkfile  ( HttpHeader *h);
    void rmfile  ( HttpHeader *h);
    void mklink ( HttpHeader *h);

    void mv     ( HttpHeader *h);

    void download ( HttpHeader *h);

    void mkicon ( HttpHeader *h);

public:
    HttpFilesystem( Http *h, int noadd = 0 );
    virtual ~HttpFilesystem();

    virtual std::string getPath() { return "file"; }
    virtual int request (HttpHeader *h);

};

#endif /* http_filesystem_mne */
