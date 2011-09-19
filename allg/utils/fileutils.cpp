#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#define WINVER 0x0500
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <errno.h>

#if defined(MACOS)
#include <sys/socket.h>
#include <sys/uio.h>
#else
#include <sys/sendfile.h>
#endif

#endif

#include "fileutils.h"

FileUtils::FileUtils()
:msg("FileUtils")
{
    createtime = accesstime = modifytime = 0;
    is_dir = 0;
    have_data = -1;
    epoch_diff = ((int64_t)134774 * (int64_t)86400);
}

FileUtils::~FileUtils()
{
}

void FileUtils::read_file_data( int werror )
{
    if ( have_data > 0 ) return;

    createtime = accesstime = modifytime = 0;
    is_dir = 0;
    have_data = -1;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    HANDLE h;
    int error_found;
    error_found = 0;

    std::string file = uncroot;
    if ( this->dir  != "" ) file += (std::string)"\\" + dir;
    if ( this->name != "" ) file += (std::string)"\\" + name;

    h = CreateFile(file.c_str(),
            FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL);

    if ( h != INVALID_HANDLE_VALUE )
    {
        if ( ! GetFileInformationByHandle(h,&data))
        {
            msg.perror(E_ATTR, "kann Fileinformation von <%s> nicht ermitteln", file.c_str());
            perror();
            error_found = 1;
        }
        CloseHandle(h);
    }
    else
    {
        if ( werror )
        {
            msg.perror(E_ATTR, "kann datei <%s> nicht öffnen", file.c_str());
            perror();
        }
        error_found = 1;
        return;
    }

    if ( error_found )
    {
        have_data = 2
        return;
    }

    have_data = 1;
    is_dir = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
    createtime = (time_t)(((((uint64_t)data.ftCreationTime.dwHighDateTime << 32)
            + data.ftCreationTime.dwLowDateTime )  ) / 10000000 - epoch_diff);
    modifytime = (time_t)(((((uint64_t)data.ftLastWriteTime.dwHighDateTime << 32)
            + data.ftLastWriteTime.dwLowDateTime )  ) / 10000000 - epoch_diff);
    accesstime = (time_t)(((((uint64_t)data.ftLastAccessTime.dwHighDateTime << 32)
            + data.ftLastAccessTime.dwLowDateTime )  ) / 10000000 - epoch_diff);

#else
    struct stat buf;

    std::string file = this->root;
    if ( this->dir  != "" ) file += (std::string)"/" + dir;
    if ( this->name != "" ) file += (std::string)"/" + name;

    if ( lstat(file.c_str(), &buf) < 0  )
    {
        if ( werror ) msg.perror(E_ATTR, "kann Fileinformation von <%s> nicht ermitteln %d", file.c_str(), werror);
        have_data = 2;
        return;
    }

    have_data = 1;
    is_dir = S_ISDIR(buf.st_mode);

    createtime = buf.st_mtime;
    modifytime = buf.st_mtime;
    accesstime = buf.st_atime;

#endif
}

int FileUtils::mk_dir(std::string root, std::string dir)
{
    have_data = -1;

#if defined(__MINGW32__) || defined(__CYGWIN__)

    std::string s;
    std::string nextdir;
    std::string::size_type i;

    i = 0;
    while ( i != std::string::npos )
    {
        i = dir.find("\\", i);
        if ( i != std::string::npos )
            nextdir = dir.substr(0,i);
        else
            nextdir = dir;

        s = root;
        if ( nextdir != "") s += "\\" + nextdir;

        if ( ! CreateDirectory(s.c_str(), NULL) )
        {
            if ( GetLastError() != ERROR_ALREADY_EXISTS )
            {
                msg.perror(E_CREATE,"Kann Verzeichnis nicht erstellen");
                msg.line( "<%s>", s.c_str());
                perror();
                return 0;
            }
        }

        if ( i != std::string::npos )
            i++;
    }

    return 1;

#else
    std::string s;
    std::string nextdir;
    std::string::size_type i;

    i = 0;
    while ( i != std::string::npos )
    {
        i = dir.find("/", i);
        if ( i != std::string::npos )
            nextdir = dir.substr(0,i);
        else
            nextdir = dir;

        s = root;
        if ( nextdir != "") s += "/" + nextdir;

        if ( ::mkdir(s.c_str(), 0777 ) < 0 )
        {
            if ( errno != EEXIST )
            {
                msg.perror(E_CREATE,"Kann Verzeichnis <%s> nicht erstellen", s.c_str());
                return 0;
            }
        }

        if ( i != std::string::npos )
            i++;
    }

#endif

    return 0;

}

int FileUtils::copy_file(int werror)
{
    have_data = -1;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::string sfile,dfile;

    sfile = uncroot;
    if ( dir != "" ) sfile += "\\" + dir;
    sfile += "\\" + name;

    mk_dir(unccopyroot, copydir);
    dfile = unccopyroot;
    if ( copydir != "" ) dfile += "\\" + copydir;
    dfile += "\\" + copyname;

    SetFileAttributes(dfile.c_str(), FILE_ATTRIBUTE_NORMAL);
    if ( ! CopyFile(sfile.c_str(), dfile.c_str(), FALSE) )
    {
        if ( werror )
        {
            msg.perror(E_COPY, "kann Datei nicht kopieren");
            msg.line( "von  <%s>", sfile.c_str());
            msg.line( "nach <%s>", dfile.c_str());
            perror();
        }
        return 0;
    }

    setFiletime(werror);
    return 1;

#else
    int read_fd;
    int write_fd;
    struct stat stat_buf;
    off_t offset = 0;

    std::string sfile,dfile;

    sfile = this->root;
    if ( dir != "" ) sfile += "/" + dir;
    sfile += "/" + name;

    mk_dir(copyroot, copydir);
    dfile = copyroot;
    if ( copydir != "" ) dfile += "/" + copydir;
    dfile += "/" + copyname;

    if ( ( read_fd = open (sfile.c_str(), O_RDONLY) ) < 0 )
    {
        msg.perror(E_COPY, "Kann Datei <%s> nicht öffnen", sfile.c_str());
        return 0;
    }
    fstat (read_fd, &stat_buf);

    if ( ( write_fd = open (dfile.c_str(), O_WRONLY | O_CREAT, stat_buf.st_mode)) < 0 )
    {
        msg.perror(E_COPY, "Kann Datei <%s> nicht öffnen", dfile.c_str());
        return 0;
    }
#if defined (MACOS)
    char buffer[10240];
    while ( ( offset = read(read_fd, buffer, sizeof(buffer)) ) > 0 )
    {
        int n,i;
        for ( n = offset; n > 0; n -= i )
        {
            i = write ( write_fd, &buffer[offset - n], n);
            if ( i < 0 )
            {
                msg.perror(E_COPY, "Kann Dateien <%s -> %s > nicht kopieren", sfile.c_str(), dfile.c_str());
                close(read_fd);
                close(write_fd);
                return 0;
            }
        }
    }

    if ( offset < 0 )
    {
        msg.perror(E_COPY, "Kann Dateien <%s -> %s > nicht kopieren", sfile.c_str(), dfile.c_str());
        close(read_fd);
        close(write_fd);
        return 0;
    }
#else
    if ( sendfile (write_fd, read_fd, &offset, stat_buf.st_size) < 0)
    {
        msg.perror(E_COPY, "Kann Dateien <%s -> %s > nicht kopieren", sfile.c_str(), dfile.c_str());

        close (read_fd);
        close (write_fd);
        return 0;
    }

#endif

    close (read_fd);
    close (write_fd);

    setFiletime(werror);
    return 1;

    return 0;
#endif
}

int FileUtils::copy_dir(int werror)
{
    have_data = -1;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::string s,d;

    s = root;
    if ( dir != "") s += "\\" + dir;
    s += "\\" + name;

    d = copyroot;
    if ( copydir != "") d += "\\" + copydir;
    d += "\\" + copyname;

    if ( ! CreateDirectoryEx(s.c_str(), d.c_str(), NULL) )
    {
        if ( werror )
        {
            msg.perror(E_COPY, "kann Ordner nicht kopieren");
            msg.line( "von  <%s>", s.c_str());
            msg.line( "nach <%s>", d.c_str());
            perror();
        }
        return 0;
    }

    setFiletime(werror);
    return 1;

#else
    std::string s;

    s = copyroot;
    if ( copydir != "") s += "/" + copydir;
    s += "/" + copyname;

    if ( ::mkdir(s.c_str(), 0777) )
    {
        if ( werror )
        {
            msg.perror(E_COPY, "kann Ordner nicht <%s> nicht erstellen", s.c_str());
        }
        return 0;
    }

    setFiletime(werror);
    return 1;
#endif

    return 0;

}

int FileUtils::remove_file(int werror)
{
    have_data = -1;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::string s;

    s = root;
    if ( dir != "") s += "\\" + dir;
    s = s + "\\" + name;

    SetFileAttributes(s.c_str(), FILE_ATTRIBUTE_NORMAL);
    if ( ! DeleteFile(s.c_str()) )
    {
        if ( werror )
        {
            msg.perror(E_DELETE, "kann Datei nicht löschen", s.c_str());
            msg.line( "<%s>", s.c_str());
            perror();
        }
        return 0;
    }

    return 1;
#else
    std::string s;

    s = root;
    if ( dir != "") s += "/" + dir;
    s = s + "/" + name;

    chflags(s.c_str(), 0);
    chmod(s.c_str(), 0777);
    if ( unlink(s.c_str()) < 0 )
    {
        if ( werror )
        {
            msg.perror(E_DELETE, "kann Datei <%s> nicht löschen", s.c_str());
        }
        return 0;
    }

    return 1;
#endif
}

int FileUtils::remove_dir(int werror)
{

#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::string s;

    s = root;
    if ( dir != "") s += "\\" + dir;
    s = s + "\\" + name;

    SetFileAttributes(s.c_str(), FILE_ATTRIBUTE_NORMAL);
    if ( ! RemoveDirectory(s.c_str()) )
    {
        if ( werror )
        {
            msg.perror(E_DELETE, "kann Ordner nicht löschen");
            msg.line( "<%s>", s.c_str());
            perror();
        }
        return 0;
    }

    return 1;

#else
    std::string s;

    s = root;
    if ( dir != "") s += "/" + dir;
    s = s + "/" + name;

    chflags(s.c_str(), 0);
    chmod(s.c_str(), 0777);
    if ( ::rmdir(s.c_str()) != 0 )
    {
        if ( werror )
        {
            msg.perror(E_DELETE, "kann Ordner <%s>nicht löschen", s.c_str());
        }
        return 0;
    }

    return 1;
#
#endif
}


void FileUtils::setRoot( std::string root)
{
#if defined(__MINGW32__) || defined(__CYGWIN__)

    have_data = -1;
    this->root = root;
    if ( this->root[0] == '\\' && this->root[1] == '\\' )
        this->uncroot = "\\\\?\\UNC" + this->root.substr(1);
    else
        this->uncroot = "\\\\?\\" + this->root;

#else
    have_data = -1;
    this->root = root;

#endif
}

void FileUtils::setCopyRoot( std::string root)
{
#if defined(__MINGW32__) || defined(__CYGWIN__)

    this->copyroot = root;
    if ( this->copyroot.substr(0,2) == "\\\\" )
        this->unccopyroot = "\\\\?\\UNC" + this->copyroot.substr(1);
    else
        this->unccopyroot = "\\\\?\\" + this->copyroot;

#else
    this->copyroot = root;
#endif
}

void FileUtils::setName( std::string d, std::string n)
{
    have_data = -1;

    this->dir  = d;
    this->name = n;

    this->copydir  = d;
    this->copyname = name;
}

void FileUtils::setCopyName( std::string d, std::string n)
{
    this->copydir  = d;
    this->copyname = name;
}

std::string FileUtils::mktime(time_t t, int local, char *format)
{
    char retval[256];

#if defined(__MINGW32__) || defined(__CYGWIN__)
    int64_t tt;
    FILETIME ft;
    FILETIME lft;
    SYSTEMTIME st;

    tt = (((int64_t)t + epoch_diff ) * 10000000) ;

    ft.dwHighDateTime = tt >> 32;
    ft.dwLowDateTime = tt;

    if ( local )
    {
        FileTimeToLocalFileTime(&ft, &lft);
        FileTimeToSystemTime(&lft, &st);
    }
    else
    {
        FileTimeToSystemTime(&ft, &st);
    }

    if ( format == NULL )
        format = (char*)"%02d.%02d.%02d %02d:%02d:%02d.%03d";

    sprintf(retval,format, st.wDay,st.wMonth, st.wYear,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    return retval;
#else
    struct tm tm;
    if ( format == NULL )
         format = (char*)"%02d.%02d.%02d %02d:%02d:%02d.%03d";

    if ( local )
        localtime_r(&t, &tm);
    else
        gmtime_r(&t, &tm);

    sprintf(retval,format, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec, 0);
    return retval;
#endif

}

void FileUtils::perror()
{
#if defined(__MINGW32__) || defined(__CYGWIN__)
    LPVOID lpMsgBuf;
    DWORD i;

    i = FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
            (LPTSTR) &lpMsgBuf,
            0,
            NULL
    );

    if ( i > 0 )
    {
        msg.line("%s", lpMsgBuf);
        LocalFree(lpMsgBuf);
    }
    else
    {
        msg.line("error code %d", GetLastError());
    }
#endif
}

int FileUtils::remove(int werror)
{
    read_file_data(werror);
    if ( have_data != 1)
        return 0;

    if ( is_dir )
        return remove_dir(werror);
    else
        return remove_file(werror);
}

int FileUtils::copy(int werror)
{
    read_file_data(werror);
    if ( have_data != 1)
        return 0;

    if ( is_dir )
        return copy_dir(werror);
    else
        return copy_file(werror);
}

int FileUtils::mkdir(int werror)
{
    have_data = -1;

#if defined(__MINGW32__) || defined(__CYGWIN__)

    std::string s;

    s = root;
    if ( dir != "") s += "\\" + dir;
    s += "\\" + name;

    if ( ! CreateDirectory(s.c_str(), NULL) )
    {
        if ( werror )
        {
            msg.perror(E_CREATE,"Kann Verzeichnis nicht erstellen");
            msg.line( "<%s>", s.c_str());
            perror();
        }
        return 0;
    }

    return 1;
#else
    std::string s;

    s = root;
    if ( dir != "") s += "/" + dir;
    s += "/" + name;

    if ( ::mkdir(s.c_str(), 0777 ) < 0 )
    {
        if ( werror )
        {
            msg.perror(E_CREATE,"Kann Verzeichnis <%s> nicht erstellen", s.c_str());
        }
        return 0;
    }

    return 1;

#endif
}

int FileUtils::mklink(int werror)
{
    have_data = -1;

#if defined(__MINGW32__) || defined(__CYGWIN__)

    std::string s;
    std::string d;

    s = uncroot;
    if ( dir != "") s += "\\" + dir;
    s += "\\" + name;

    d = unccopyroot;
    if ( copydir != "") d += "\\" + copydir;
    d += "\\" + copyname;

    if ( ! CreateHardLink(d.c_str(), s.c_str(), NULL) )
    {
        if ( werror )
        {
            msg.perror(E_CREATE,"Kann hardlink nicht erstellen");
            msg.line( "von  <%s>", s.c_str());
            msg.line( "nach <%s>", d.c_str());
            perror();
        }
        return 0;
    }

    return 1;
#else

    std::string s;
    std::string d;

    s = this->root;
    if ( dir != "") s += "/" + dir;
    s += "/" + name;

    d = this->copyroot;
    if ( copydir != "") d += "/" + copydir;
    d += "/" + copyname;

    if ( link(s.c_str(), d.c_str()) != 0 )
    {
        if ( werror )
        {
            msg.perror(E_CREATE,"Kann hardlink <%s -> %s> nicht erstellen", d.c_str(), s.c_str());
        }
        return 0;
    }

    return 1;
#endif
}

int FileUtils::setFiletime(int werror)
{

    read_file_data(werror);
    if ( have_data != 1)
        return 0;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    HANDLE sh,dh;
    std::string str;

    str = unccopyroot;
    if ( dir != "" ) str += "\\" + copydir;
    str += "\\" + copyname;

    sh = CreateFile(str.c_str(),
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL);

    if ( sh == INVALID_HANDLE_VALUE )
    {
        msg.perror(E_OPEN, "kann datei nicht öffnen");
        msg.line( "<%s>", str.c_str());
        perror();
        return 0;
    }

    str = uncroot;
    if ( dir != "" ) str += "\\" + dir;
    str += "\\" + name;

    dh = CreateFile(str.c_str(),
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL);

    if ( dh == INVALID_HANDLE_VALUE )
    {
        msg.perror(E_OPEN, "kann datei nicht öffnen");
        msg.line( "<%s>", str.c_str());
        perror();
        CloseHandle(sh);
        return 0;
    }

    SetFileTime(sh, &data.ftCreationTime, &data.ftLastAccessTime, &data.ftLastWriteTime);
    GetFileTime(sh, &data.ftCreationTime, &data.ftLastAccessTime, &data.ftLastWriteTime);

    SetFileTime(dh, &data.ftCreationTime, &data.ftLastAccessTime, &data.ftLastWriteTime);
    GetFileTime(dh, &data.ftCreationTime, &data.ftLastAccessTime, &data.ftLastWriteTime);

    SetFileTime(sh, &data.ftCreationTime, &data.ftLastAccessTime, &data.ftLastWriteTime);

    createtime = (time_t)(((((uint64_t)data.ftCreationTime.dwHighDateTime << 32)
            + data.ftCreationTime.dwLowDateTime )  ) / 10000000 - epoch_diff);
    modifytime = (time_t)(((((uint64_t)data.ftLastWriteTime.dwHighDateTime << 32)
            + data.ftLastWriteTime.dwLowDateTime )  ) / 10000000 - epoch_diff);
    accesstime = (time_t)(((((uint64_t)data.ftLastAccessTime.dwHighDateTime << 32)
            + data.ftLastAccessTime.dwLowDateTime )  ) / 10000000 - epoch_diff);

    CloseHandle(sh);
    CloseHandle(dh);
    return 1;
#else
    std::string sfile, dfile;
    struct utimbuf buf;

    sfile = this->root;
    if ( dir != "" ) sfile += "/" + dir;
    sfile += "/" + name;

    dfile = this->copyroot;
    if ( dir != "" ) dfile += "/" + copydir;
    dfile += "/" + copyname;

    buf.actime = accesstime;
    buf.modtime = modifytime;

    utime(sfile.c_str(), &buf);
    utime(dfile.c_str(), &buf);

    return 1;
#endif
}
