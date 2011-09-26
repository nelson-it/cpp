#ifndef fileutils_mne
#define fileutils_mne

#include <string>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <windows.h>
#include <inttypes.h>
#define __in
#define __out
#define __in_opt
#include <msadd/fileextd.h>
#endif

#include <message/message.h>
class FileUtils
{

    Message msg;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    BY_HANDLE_FILE_INFORMATION data;
#endif

protected:
    enum ERROR_TYPES
    {
        E_OK = 0,
        E_OPEN,
        E_COPY,
        E_CREATE,
        E_ATTR,
        E_DELETE
    };

#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::string uncroot;
    std::string unccopyroot;
#endif

    std::string root;
    std::string dir;
    std::string name;

    std::string copyroot;
    std::string copydir;
    std::string copyname;

    int is_dir;
    int have_data;

    time_t createtime;
    time_t accesstime;
    time_t modifytime;

    int64_t epoch_diff;

    virtual void read_file_data(int werror = 1);

    virtual int copy_file(int werror);
    virtual int copy_dir(int werror);

    virtual int remove_file(int werror);
    virtual int remove_dir(int werror);

public:
    FileUtils();
    virtual ~FileUtils();

    void setRoot(std::string dir);
    void setCopyRoot(std::string dir);

    void setName    ( std::string dir, std::string name);
    void setCopyName( std::string dir, std::string name);

    std::string getRoot() { return root; };
    std::string getDir()  { return dir;  };
    std::string getName() { return name; };
#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::string getFullname() { if ( dir == "" ) return name; else return dir + "\\" + name; }
#else
    std::string getFullname() { if ( dir == "" ) return name; else return dir + "/" + name; }
#endif
    std::string getCopyRoot() { return copyroot; };
    std::string getCopyDir()  { return copydir;  };
    std::string getCopyName() { return copyname; };

    time_t getCreatetime(int werror = 1)
    { read_file_data(werror); return createtime; }
    time_t getAccesstime(int werror = 1)
    { read_file_data(werror); return accesstime; }
    time_t getModifytime(int werror = 1)
    { read_file_data(werror); return modifytime; }

    int isDir( int werror = 1 )  { read_file_data(werror); return is_dir;    }
    int exists( int werror = 1)  { read_file_data(werror); return ( have_data == 1 ); }

    virtual int mk_dir(std::string root, std::string dir);

    int copy(int werror = 1);
    int remove(int werror = 1);
    int mkdir(int werror = 1);
    int mklink(int werror = 1);

    virtual int setFiletime(int werror = 1);
    std::string mktime(time_t, int local = 0, char *format = NULL);

    void perror();
};


#endif /* fileutils_mne */

