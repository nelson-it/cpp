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
        E_ROOT,
        E_NEEDNAME,

        E_CREATEFILE,
        E_DELFILE,
        E_CREATELINK,

        E_FILEEXISTS,
        E_FILE_OPEN,

        E_WRONGNAME,

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
    virtual std::string getDir(std::string dir, int newpath = 0 );

    //virtual std::string check_path(std::string dir, std::string name, int needname = 1, int errormsg = 1, std::string *result = NULL );
    //virtual std::string check_path(HttpHeader *h, std::string name, int needname = 1 , int errormsg = 1 , std::string *result = NULL );

    virtual int findfile(HttpHeader *h);

    std::string root;
    std::string rootnew;

    std::string rootpath;
    std::string rootpathnew;

    std::string cacheroot;

    //struct stat statbuf;

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

    virtual int  quicksort_check(FileData *data1, FileData *data2, FileDataSort qs_type);
    virtual void quicksort(std::vector<FileData> &sort, FileDataSort qs_type, int left, int right);

    virtual void readdir(std::string dirname, int pointdir );
    virtual int hassubdirs(std::string path, int pointdir);

    std::string mkfile ( HttpHeader *h);
    void mkfile_json   (HttpHeader *h);

    void ls     ( HttpHeader *h);
    void mkdir  ( HttpHeader *h);

    int  rmdir      ( HttpHeader *h);
    void rmdir_json ( HttpHeader *h);

    int  rmfile      ( HttpHeader *h);
    void rmfile_json ( HttpHeader *h);

    void mklink ( HttpHeader *h);

    void mv ( HttpHeader *h);

    void download ( HttpHeader *h);

    void mkicon ( HttpHeader *h);

public:
    HttpFilesystem( Http *h, int noadd = 0 );
    virtual ~HttpFilesystem();

    virtual std::string getPath() { return "file"; }
    virtual int request (HttpHeader *h);

};

#endif /* http_filesystem_mne */
