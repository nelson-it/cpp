#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <magic.h>
#include <poll.h>
#include <sys/wait.h>

#include <utils/tostring.h>
#include <utils/php_exec.h>

#include <string>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <winsock2.h>
#include <windows.h>

#define DIRSEP   "\\"
#define lstat stat

char *realpath(const char *path, char resolved_path[PATH_MAX])
{
  char *return_path = 0;

  if (path) //Else EINVAL
  {
    if (resolved_path)
    {
      return_path = (char *)resolved_path;
    }
    else
    {
      //Non standard extension that glibc uses
      return_path = (char *)malloc(PATH_MAX);
    }

    if (return_path) //Else EINVAL
    {
      //This is a Win32 API function similar to what realpath() is supposed to do
      size_t size = GetFullPathNameA(path, PATH_MAX, return_path, 0);

      //GetFullPathNameA() returns a size larger than buffer if buffer is too small
      if (size > PATH_MAX)
      {
        if (return_path != resolved_path) //Malloc'd buffer - Unstandard extension retry
        {
          size_t new_size;

          free(return_path);
          return_path = (char *)malloc(size);

          if (return_path)
          {
            new_size = GetFullPathNameA(path, size, return_path, 0); //Try again

            if (new_size > size) //If it's still too large, we have a problem, don't try again
            {
              free(return_path);
              return_path = 0;
              errno = ENAMETOOLONG;
            }
            else
            {
              size = new_size;
            }
          }
          else
          {
            //I wasn't sure what to return here, but the standard does say to return EINVAL
            //if resolved_path is null, and in this case we couldn't malloc large enough buffer
            errno = EINVAL;
          }
        }
        else //resolved_path buffer isn't big enough
        {
          return_path = 0;
          errno = ENAMETOOLONG;
        }
      }

      //GetFullPathNameA() returns 0 if some path resolve problem occured
      if (!size)
      {
        if (return_path != resolved_path) //Malloc'd buffer
        {
          free(return_path);
        }

        return_path = 0;

        //Convert MS errors into standard errors
        switch (GetLastError())
        {
          case ERROR_FILE_NOT_FOUND:
            errno = ENOENT;
            break;

          case ERROR_PATH_NOT_FOUND: case ERROR_INVALID_DRIVE:
            errno = ENOTDIR;
            break;

          case ERROR_ACCESS_DENIED:
            errno = EACCES;
            break;

          default: //Unknown Error
            errno = EIO;
            break;
        }
      }

      //If we get to here with a valid return_path, we're still doing good
      if (return_path)
      {
        struct stat stat_buffer;

        //Make sure path exists, stat() returns 0 on success
        if (stat(return_path, &stat_buffer))
        {
          if (return_path != resolved_path)
          {
            free(return_path);
          }

          return_path = 0;
          //stat() will set the correct errno for us
        }
        //else we succeeded!
      }
    }
    else
    {
      errno = EINVAL;
    }
  }
  else
  {
    errno = EINVAL;
  }

  return return_path;
}
#else
#define DIRSEP   "/"
#endif

#include <argument/argument.h>

#include "http_filesystem.h"


HttpFilesystem::HttpFilesystem(Http *h, int noadd )
               : HttpProvider(h),
                 msg("HttpFilesystem")
{
    Argument a;

    this->cacheroot = (std::string)a["EmbedwebHttpFileCacheroot"];
#if defined(__MINGW32__) || defined(__CYGWIN__)
    if (this->cacheroot[1] != ':')
#else
    if ( this->cacheroot[0] != '/' )
#endif
        this->cacheroot = (std::string)a["projectroot"] + "/" + this->cacheroot;

    subprovider["ls.json"]     = &HttpFilesystem::ls;
    subprovider["mkdir.json"]  = &HttpFilesystem::mkdir;
    subprovider["rmdir.json"]  = &HttpFilesystem::rmdir_json;
    subprovider["mkfile.json"] = &HttpFilesystem::mkfile_json;
    subprovider["rmfile.json"] = &HttpFilesystem::rmfile_json;
    subprovider["mklink.json"] = &HttpFilesystem::mklink;
    subprovider["mv.json"] = &HttpFilesystem::mv;
    subprovider["download.html"] = &HttpFilesystem::download;
    subprovider["stream.dat"] = &HttpFilesystem::stream;
    subprovider["mk_icon.php"] = &HttpFilesystem::mkicon;

    if ( noadd == 0 )
        h->add_provider(this);
}

HttpFilesystem::~HttpFilesystem()
{
}

int HttpFilesystem::findfile(HttpHeader *h)
{
    struct stat s;
    std::string file;
    std::string name;
    unsigned int j;

    if ( h->dirname != "/" )
      name = h->dirname + "/" + h->filename;
    else
      name = "/" + h->filename;

        for (j=0; j<h->serverpath.size(); j++)
        {
            file = h->serverpath[j] + "/file" + name;
            msg.pdebug(3, "check %s", file.c_str());
            if (stat(file.c_str(), &s) == 0)
            {
                h->status = 200;
                if (file.find(".php") == (file.size() - 4 ))
                {
                    h->age = 0;
                    PhpExec(file, h);
                    if ( h->error_found )
                    {
                        h->content_type = "text/json";
                        del_content(h);
                        add_content(h, " { \"result\" : \"error\" ");
                    }
                    return 1;
                }
                else
                {
                    FILE *c = fopen(file.c_str(), "rb");
                    if ( c != NULL )
                    {
                        h->status = 200;
                        contentf(h, c);
                        fclose(c);
                        h->translate = 1;
                        return 1;
                    }
                    else
                    {
                        if ( h->vars["ignore_notfound"] == "" )
                            msg.perror(E_FILE_OPEN, "kann datei <%s> nicht öffnen", name.c_str());
                        return 0;
                    }
                }
            }
        }
    if ( h->vars["ignore_notfound"] == "" )
        msg.perror(E_FILENOTFOUND, "kann datei <%s> nicht finden", name.c_str());
    return 0;

}

int HttpFilesystem::request(HttpHeader *h)
{

    SubProviderMap::iterator i;

    getRoot(h);
    if ( this->rootpath == "" )
    {
        msg.perror(E_ROOT, "Wurzel unbekannt");
        add_content(h, "{ \"result\" : \"error\"");
        return 1;
    }

    if ( (i = subprovider.find(h->filename)) != subprovider.end() )
    {
        h->status = 200;
        h->content_type = "text/json";

        if ( h->vars["filenameInput"].find(DIRSEP) != std::string::npos || h->vars["filenameInput.old"].find(DIRSEP) != std::string::npos )
        {
            msg.perror(E_WRONGNAME, "Dateinamen sind im falschen Format");
            add_content(h, "{ \"result\" : \"error\"");
            return 1;
        }

        (this->*(i->second))(h);
        return 1;
    }

    (*(h->vars.p_getExtraVars()))["root"] = rootpath;
    (*(h->vars.p_getExtraVars()))["realpath"] = this->getDir(h->vars["dirInput.old"]);

    if ( h->error_messages.size() )
    {
        h->status = 200;
        h->content_type = "text/json";
        add_content(h, "{ \"result\" : \"error\"");
        return 1;
    }

    return findfile(h);

}

std::string HttpFilesystem::getRoot(HttpHeader *h )
{
    std::map<std::string,std::string>::iterator m;
    this->root    = h->vars["rootInput.old"];
    this->rootnew = h->vars["rootInput"];

    for (m = h->datapath.begin(); m != h->datapath.end(); ++m )
    {
        msg.pdebug(D_ROOTDIRS, "check %s %s", root.c_str(), m->first.c_str());
        if ( m->first == root ) break;
    }

    if ( m != h->datapath.end() )
    {
#if defined(__MINGW32__) || defined(__CYGWIN__)
        if ( m->second[1] == ':' )
#else
            if ( m->second[0] == '/' )
#endif

            {
                msg.pdebug(D_ROOTDIRS, "found %s",  m->second.c_str());
                this->rootpath =  m->second + DIRSEP;
            }
            else
            {
                msg.pdebug(D_ROOTDIRS, "found %s", (h->dataroot + "/" + m->second).c_str());
                this->rootpath =  h->dataroot + DIRSEP + m->second + DIRSEP;
            }
    }
    if ( rootnew != "" )
    {

        for (m = h->datapath.begin(); m != h->datapath.end(); ++m )
        {
            msg.pdebug(D_ROOTDIRS, "check %s %s", rootnew.c_str(), m->first.c_str());
            if ( m->first == rootnew ) break;
        }

        if ( m != h->datapath.end() )
        {
#if defined(__MINGW32__) || defined(__CYGWIN__)
            if ( m->second[1] == ':' )
#else
                if ( m->second[0] == '/' )
#endif
                {
                    msg.pdebug(D_ROOTDIRS, "found %s",  m->second.c_str());
                    this->rootpathnew =  m->second + DIRSEP;
                }
                else
                {
                    msg.pdebug(D_ROOTDIRS, "found %s", (h->dataroot + "/" + m->second).c_str());
                    this->rootpathnew =  h->dataroot + DIRSEP + m->second + DIRSEP;
                }
        }
    }
    else
    {
        this->rootpathnew = this->rootpath;
    }

    return this->rootpath;

}

std::string HttpFilesystem::getDir(std::string dir, int newpath )
{
    char rpath[PATH_MAX + 1];
    char *resolvpath;

    std::string path = ( newpath ) ? rootpathnew : rootpath;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::replace( root.begin(), root.end(), '/', '\\');
    std::replace( dir.begin(), dir.end(), '/', '\\');
#endif

    rpath[0] = '\0';
    if ( path == ""
         || ( resolvpath = realpath((path + dir).c_str(), rpath)) == NULL
         || strstr(rpath, path.substr(0,path.length() - 1).c_str()) == NULL )
    {
    	msg.pdebug(D_ROOTDIRS, "rpath: %s, root: %s, dir: %s error: %s", rpath, root.c_str(), dir.c_str(), strerror(errno));
        msg.perror(E_FILENOTFOUND, "Der Ordner <%s> wurde nicht gefunden", ((( newpath ) ? rootnew : root ) + ":" + dir).c_str());
        return "";
    }

    return std::string(rpath) + "/";
}

int HttpFilesystem::quicksort_check(FileData *data1, FileData *data2, FileDataSort qs_type )
{
   switch(qs_type)
   {
   case FD_CREATE:
       return ( data1->statbuf.st_ctime < data2->statbuf.st_ctime );

   case FD_MOD:
       return ( data1->statbuf.st_mtime < data2->statbuf.st_mtime );

   case FD_ACCESS:
       return ( data1->statbuf.st_atime < data2->statbuf.st_atime );

   default:
       return ( strcmp(data1->name.c_str(), data2->name.c_str()) < 0 );
   }
}

void HttpFilesystem::quicksort(std::vector<FileData> &sort, FileDataSort qs_type, int left, int right)
{
    int i,j;
    FileData *x;
    FileData temp;

    if ( left >= right ) return;
    // x = &(sort[(left + right )/2]);
    x = &(sort[right]);

    i = left-1;
    j = right;

    while ( 1 )
    {
        while ( quicksort_check(&(sort[++i]), x, qs_type) );
        while ( quicksort_check(x, &(sort[--j]), qs_type) && j > i );
        if ( i >= j )
            break;
        temp = sort[i];
        sort[i] = sort[j];
        sort[j] = temp;
    }

    temp = sort[i];
    sort[i] = sort[right];
    sort[right] = temp;

    quicksort(sort, qs_type, left, i-1);
    quicksort(sort, qs_type, i+1, right);
}

void HttpFilesystem::readdir(std::string dirname, int pointdir )
{
    files.clear();
    dirs.clear();

#if defined(__MINGW32__) || defined(__CYGWIN__)
    HANDLE hh;
    WIN32_FIND_DATA d;
    std::string str;

    str = dirname + "\\*";
    hh = FindFirstFile(str.c_str(), &d);
    if (hh == INVALID_HANDLE_VALUE)
    {
        msg.perror(E_FILENOTFOUND, "Der Ordner <%s> wurde nicht gefunden", ( root + ":" + dirname ).c_str());
        return;
    }

    do
    {
        if ( ( !pointdir && (std::string(d.cFileName))[0] == '.' )|| (std::string(d.cFileName)) == "." || (std::string(d.cFileName)) == ".." ) continue;

        FileData data;
        data.name = d.cFileName;
        lstat((dirname + "\\" + data.name).c_str(), &data.statbuf);
        if ( d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            dirs.push_back(data);
        else
            files.push_back(data);
    }
    while (FindNextFile(hh, &d) );
    FindClose(hh);
#else
    DIR *dp;
    struct dirent *dirp;
    magic_t myt = magic_open(MAGIC_MIME);
    magic_load(myt, NULL);

    if((dp  = opendir(dirname.c_str())) == NULL)
    {
        msg.perror(E_FILENOTFOUND, "Der Ordner <%s> wurde nicht gefunden", dirname.c_str());
        return;
    }

    while (( dirp = ::readdir(dp) ) != NULL )
    {
        if ( ( !pointdir && (std::string(dirp->d_name))[0] == '.' ) || (std::string(dirp->d_name)) == "." || (std::string(dirp->d_name)) == ".." ) continue;
        {
            FileData data;
            data.name = dirp->d_name;
            lstat((dirname + "/" + data.name).c_str(), &data.statbuf);

            if ( dirp->d_type == DT_DIR )
                dirs.push_back(data);
            else
            {
                const char *mime = magic_file(myt,(dirname + "/" + data.name).c_str());
                data.mime = ( mime != NULL ) ? mime : "application/octet-stream";
                files.push_back(data);
            }
        }
    }

    magic_close(myt);
    closedir(dp);


    return;
#endif
}

int HttpFilesystem::hassubdirs(std::string dir, int pointdir)
{

#if defined(__MINGW32__) || defined(__CYGWIN__)
    HANDLE hh;
    WIN32_FIND_DATA d;
    std::string str;

    str = dir + "\\*";
    hh = FindFirstFile(str.c_str(), &d);
    if (hh == INVALID_HANDLE_VALUE)
    {
        msg.perror(E_FILENOTFOUND, "Der Ordner <%s> wurde nicht gefunden", ( root + ":" + dir ).c_str());
        return 0;
    }

    do
    {
        if ( ( !pointdir && (std::string(d.cFileName))[0] == '.' )|| (std::string(d.cFileName)) == "." || (std::string(d.cFileName)) == ".." ) continue;
        if ( d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
        {
            FindClose(hh);
            return 1;
        }
    }
    while (FindNextFile(hh, &d) );
    FindClose(hh);
    return 0;
#else
    DIR *dp;
    struct dirent *dirp;

    if((dp  = opendir(dir.c_str())) == NULL)
    {
        msg.perror(E_FILENOTFOUND, "Der Ordner <%s> wurde nicht gefunden", dir.c_str());
        return 0;
    }

    while (( dirp = ::readdir(dp) ) != NULL )
    {
        if ( ( !pointdir && (std::string(dirp->d_name))[0] == '.' ) || (std::string(dirp->d_name)) == "." || (std::string(dirp->d_name)) == ".." ) continue;
        if ( dirp->d_type == DT_DIR )
        {
            closedir(dp);
            return 1;
        }
    }

    closedir(dp);
    return 0;
#endif
}

void HttpFilesystem::ls(HttpHeader *h)
{
    unsigned int i;
    std::string str;
    std::string dir,fdir;

    std::vector<FileData>::iterator is;

    int onlydir          = ( h->vars["noleaf"] != "" && h->vars["noleaf"] != "0" );
    FileDataSort qs_type = (FileDataSort)atoi(h->vars["sorttyp"].c_str());

    dir = getDir(h->vars["dirInput.old"]);

    if ( ! h-> error_found )
    {
        fdir = dir.substr(rootpath.size());
        readdir( dir, h->vars["pointdirInput.old"] == "1");

        quicksort( dirs,  qs_type, 0,  dirs.size() - 1);
        quicksort( files, qs_type, 0, files.size() - 1);
    }

    if ( h->error_found )
    {
        add_content(h, "{ \"result\" : \"error\"");
        return;
    }

    h->content_type = "text/json";

    add_content(h, "{  \"ids\" : [\"menuid\", \"item\", \"action\", \"typ\", \"pos\" ],\n"
                   "  \"typs\" : [ 2, 2, 2, 2, 2 ],\n"
                   "  \"labels\" : [\"menuid\", \"item\", \"action\", \"typ\", \"pos\" ],\n"
                   "  \"formats\" : [ \"\",\"\",\"\",\"\",\"\" ],\n"
                   "  \"values\" : [\n");

    if ( dir != "" && dir.substr(dir.length() - 1) != std::string(DIRSEP) )
        dir = dir + DIRSEP;

    const char* format = "[\"%s\", \"%s\",\"%s\",\"%s\",\"%d\" ]";
    std::string komma; komma = "";

    i = 0;
    for ( is= dirs.begin(); is != dirs.end(); ++is )
    {
        char str[1024];
        str[sizeof(str) - 1] =  '\0';
        std::string fullname = fdir + (*is).name;
        const char *action = "submenu";
        const char *leaf = "";

        if ( onlydir && hassubdirs(rootpath + fullname, 0) == 0 )
            leaf = "leaf";

        snprintf(str, sizeof(str) - 1, "\"root\" : \"%s\", \"fullname\" : \"%s\", \"name\" : \"%s\", \"createtime\" : %ld, \"modifytime\" : %ld, \"accesstime\" : %ld, \"filetype\": \"dir\", \"mime\" : \"\"", root.c_str(), fullname.c_str(), (*is).name.c_str(), (*is).statbuf.st_ctime, (*is).statbuf.st_mtime, (*is).statbuf.st_atime );
        add_content(h, (komma + format).c_str(), fullname.c_str(), (*is).name.c_str(), ToString::mkjson("{ \"action\" : \"" + std::string(action) + "\"," "  \"parameter\" : [ \"" +  ToString::mkjson(fullname.c_str()) + "\",\"" + (*is).name.c_str() + "\", { " + str + " } ] }").c_str(), leaf, i++);

        komma = ',';

    }

    for ( is= files.begin(); !onlydir && is != files.end(); ++is )
    {
        char str[1024];
        const char *ft;
        str[sizeof(str) - 1] =  '\0';

        switch ((*is).statbuf.st_mode & S_IFMT) {
         case S_IFREG:  ft = "file";   break;
         case S_IFBLK:  ft = "bdev";   break;
         case S_IFCHR:  ft = "cdev";   break;
         case S_IFIFO:  ft = "fifo";   break;
#if ! defined(__MINGW32__) && ! defined(__CYGWIN__)
         case S_IFLNK:  ft = "slink";  break;
         case S_IFSOCK: ft = "socket"; break;
#endif
         default:       ft = "file";   break;
         }
        std::string fullname = fdir + (*is).name;
        snprintf(str, sizeof(str) - 1, "\"root\" : \"%s\", \"fullname\" : \"%s\", \"name\" : \"%s\", \"createtime\" : %ld, \"modifytime\" : %ld, \"accesstime\" : %ld, \"filetype\": \"%s\", \"mime\" : \"%s\"", root.c_str(), fullname.c_str(), (*is).name.c_str(), (*is).statbuf.st_ctime, (*is).statbuf.st_mtime, (*is).statbuf.st_atime, ft, (*is).mime.c_str() );
        add_content(h, (komma + format).c_str(), fullname.c_str(), (*is).name.c_str(), ToString::mkjson("{ \"action\" : \"show\"," "  \"parameter\" : [ \"" +  ToString::mkjson(fullname.c_str()) + "\",\"" + (*is).name.c_str() + "\", { " + str + " } ] }").c_str(), "leaf", i++ );

        komma = ',';
    }

    add_content(h, "]");
    return;

}

void HttpFilesystem::mkdir(HttpHeader *h)
{
    std::string dir = getDir(h->vars["dirInput.old"]);
    std::string name = h->vars["filenameInput"];

    h->content_type = "text/json";

    if ( dir == "" || name == "" )
    {
        add_content(h, "{ \"result\" : \"error\"");
        return;
    }

#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( ! CreateDirectory((dir + DIRSEP + name).c_str(), NULL) )
    {
        msg.perror(E_CREATEFILE, "Fehler während des Erstellens eines Ordners");
        add_content(h, "{ \"result\" : \"error\"");
        return;
    }
#else
    if ( ::mkdir((dir + DIRSEP + name).c_str(), 0777 ) != 0 )
    {
        std::string str = msg.getSystemerror(errno);
        msg.perror(E_CREATEFILE, "Fehler während des Erstellens eines Ordners");
        msg.line("%s %s", (dir + DIRSEP + name).c_str(), str.c_str());
        add_content(h, "{ \"result\" : \"error\"");
        return;
    }

    mode_t mask;
    mask = umask(0);
    umask(mask);
    chmod((dir + DIRSEP + name).c_str(), (0777 & ~ mask) );

#endif
    add_content(h, "{ \"result\" : \"ok\"");
}

int HttpFilesystem::rmdir(HttpHeader *h)
{
    std::string name = getDir(h->vars["dirInput.old"]);

    if (  name == "" )
        return -1;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    SetFileAttributes(name.c_str(), FILE_ATTRIBUTE_NORMAL);
    if ( ! RemoveDirectory(name.c_str()) )
    {
        msg.perror(E_DELFILE, "Fehler während des Löschen eines Ordners");
        return -1;
    }
#else
    if ( ::rmdir(name.c_str()) != 0 )
    {
        std::string str = msg.getSystemerror(errno);
        msg.perror(E_DELFILE, "Fehler während des Löschen eines Ordners");
        msg.line("%s %s", name.c_str(), str.c_str());

        return -1;
    }
#endif
    return 0;
}

void HttpFilesystem::rmdir_json (  HttpHeader *h)
{
    h->content_type = "text/json";
    add_content(h, ( this->rmdir(h) < 0 ) ?  "{ \"result\" : \"error\"" : "{ \"result\" : \"ok\"");
}


void HttpFilesystem::mv(HttpHeader *h)
{
    std::string dirold = getDir(h->vars["dirInput.old"]);
    std::string dir = getDir(h->vars["dirInput"], 1);

    std::string oldname = h->vars["filenameInput.old"];
    std::string newname = h->vars["filenameInput"];
    int overwrite = ( h->vars["overwrite"] != "" && h->vars["overwrite"] != "0" );

    h->content_type = "text/json";

    if ( ( dirold == "" && oldname == "" ) || ( dir == "" && newname == "" ) )
    {
        msg.perror(E_NEEDNAME, "Der Name der Datei darf nicht leer sein");
    }

    if ( ! h->error_found )
    {
#if defined(__MINGW32__) || defined(__CYGWIN__)
        if ( ! MoveFile((dirold + DIRSEP + oldname).c_str(), (dir + DIRSEP + newname).c_str()) )
        {
            msg.perror(E_CREATEFILE, "Fehler während des Umbenennes eines Ordner oder Datei");
        }
#else
        if ( !overwrite && newname != "" && access((dir + DIRSEP + newname).c_str(), F_OK) == 0 )
        {
            msg.perror(E_CREATEFILE, "Datei existiert");
        }

        if ( ::rename((dirold + DIRSEP + oldname).c_str(), (dir + DIRSEP + newname).c_str()) != 0 )
        {
            std::string str = msg.getSystemerror(errno);
            msg.perror(E_CREATEFILE, "Fehler während des Umbenennes eines Ordner oder Datei");
            msg.line("%s", str.c_str());
        }
#endif
    }
    add_content(h, ( h->error_found ) ?  "{ \"result\" : \"error\"" : "{ \"result\" : \"ok\"");
}

std::string HttpFilesystem::mkfile(HttpHeader *h)
{
    std::string str;
    std::string path;

    str = h->vars.getFile("dataInput");
    if ( str == "" )
    {
        return msg.get("Keine Daten gefunden");
    }
    else
    {
#if defined(__MINGW32__) || defined(__CYGWIN__)
        std::string name = h->vars["filenameInput"];
        if ( getDir(h->vars["dirInput.old"]) == "" || name == "" )
        {
            str = msg.get("Benötige einen Dateinamen");
        }

        if ( ! CopyFile(str.c_str(), (path + DIRSEP + name).c_str(), FALSE) )
        {
            str = msg.getSystemerror(errno);
        }
#else
        int file1;
        if ( ( file1 = open(str.c_str(), O_RDONLY)) < 0 )
        {
            return msg.getSystemerror(errno);
        }
        else
        {
            std::string name = h->vars["filenameInput"];
            int overwrite = ( h->vars["overwrite"] != "" && h->vars["overwrite"] != "0" );

            if ( (path = getDir(h->vars["dirInput.old"])) == "" || name == "" )
            {
                return msg.get("Benötige einen Dateinamen");
            }
            else
            {
                std::string file = path + DIRSEP + name;
                if ( !overwrite && access( file.c_str() , F_OK ) == 0 )
                {
                    return msg.get("Datei existiert und wird nicht überschrieben");
                }

                ::unlink(file.c_str());
                if ( ::rename(str.c_str(), (path + DIRSEP + name).c_str()) != 0 )
                {
                    std::string str = msg.getSystemerror(errno);
                    msg.perror(E_CREATEFILE, "Fehler während des Estellens einer Datei");
                    msg.line("%s", str.c_str());
                }

/*                int file2 = open(file.c_str(), O_WRONLY | O_CREAT, 0666 );
                if ( file2 < 0 )
                {
                    str = msg.getSystemerror(errno);
                    close(file1);
                    return str;
                }
                else
                {
                    str = "";
                    mode_t mask;
                    mask = umask(0);
                    umask(mask);
                    fchmod(file2, (0666 & ~ mask));
                    int i,n;
                    char buf[1024];
                    while ( ( i = read(file1, &buf, sizeof(buf))) > 0 )
                    {
                        while ( i > 0 && ( n = write(file2, &buf, i)) > 0 ) i -= n;
                        if ( i != 0 )
                        {
                            str = msg.getSystemerror(errno);
                            break;
                        }
                    }
                    close(file1);
                    close(file2);
                    if ( str != "" ) return str;
                }
*/
            }
        }
#endif
    }
    return "ok";
}

void HttpFilesystem::mkfile_json(HttpHeader *h)
{
    h->status = 200;
    h->content_type = "text/json";

    std::string str = mkfile(h);
    if ( str != "ok" )
    {
        msg.ignore_lang = 1;
        msg.perror(E_FILENOTFOUND, str.c_str());
        msg.ignore_lang = 0;

        add_content(h, "{ \"result\" : \"error\"");
    }
    else
    {
        add_content(h, "{ \"result\" : \"ok\"");
    }


}

int HttpFilesystem::rmfile(HttpHeader *h)
{
    std::string name = getDir(h->vars["dirInput.old"]) + "/" + h->vars["filenameInput.old"];

    if (  name == "" )
        return -1;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( ! DeleteFile(name.c_str()) )
    {
        msg.perror(E_DELFILE, "Fehler während des Löschen einer Datei");
        return -1;
    }
#else
    if ( ::unlink(name.c_str()) != 0 )
    {
        std::string str = msg.getSystemerror(errno);
        msg.perror(E_DELFILE, "Fehler während des Löschen einer Datei");
        msg.line("%s %s", name.c_str(), str.c_str());

        return -1;
    }
#endif
    return 0;
}

void HttpFilesystem::rmfile_json (  HttpHeader *h)
{
    h->content_type = "text/json";
    add_content(h, ( this->rmfile(h) < 0 ) ?  "{ \"result\" : \"error\"" : "{ \"result\" : \"ok\"");
}

void HttpFilesystem::mklink(HttpHeader *h)
{
    std::string root,dir1,dir2,file1,file2;
    int result = 0;
    struct stat statbuf;

    h->content_type = "text/json";

    dir1 = this->getDir(h->vars["dirInput.old"]);
    dir2 = this->getDir(h->vars["dirInput"], 1 );

    file1 = dir1 + "/" +  h->vars["filenameInput.old"];
    file2 = dir2 + "/" +  h->vars["filenameInput"];

    if ( lstat(file1.c_str(), &statbuf) != 0  )
    {
        msg.perror(E_FILEEXISTS, "Datei existiert nicht");
        result = -1;
    }

    if ( lstat(file2.c_str(), &statbuf) == 0  && h->vars["overwriteInput"] == "" )
    {
        msg.perror(E_FILEEXISTS, "Datei existiert schon");
        result = -1;
    }


#if defined(__MINGW32__) || defined(__CYGWIN__)

    if ( ! result )
    {
        if ( h->vars["symlink"] != "" )
            result = ! CreateHardLink(file2.c_str(), file1.c_str(), NULL);
        else
            result = ! CreateSymbolicLink(file2.c_str(), file1.c_str(), 0);
    }

#else

    if ( ! result )
    {
        if ( h->vars["symlink"] != "" )
            result = symlink(file1.c_str(), file2.c_str());
        else
            result = link(file1.c_str(), file2.c_str());

        if ( result )
            msg.perror(E_CREATELINK, "Kann Link nicht erstellen");
    }

#endif

    add_content(h, ( result != 0 ) ?  "{ \"result\" : \"error\"" : "{ \"result\" : \"ok\"");
}

void HttpFilesystem::download(HttpHeader *h)
{
    std::string name;
    FILE *f;

    name = getDir(h->vars["dirInput.old"]) + "/" + h->vars["filenameInput.old"];
    if ( name != "" && ( f = fopen(name.c_str(), "r")) != NULL )
    {
        magic_t myt = magic_open(MAGIC_MIME);
        magic_load(myt, NULL);

        char buffer[10240];
        const char *magic = magic_file(myt,name.c_str());

        h->content_type = ( magic != NULL ) ? magic : "application/octet-stream";
        snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", h->vars.url_decode(h->vars["filenameInput.old"].c_str()).c_str());
        buffer[sizeof(buffer) -1] = '\0';
        h->extra_header.push_back(buffer);

        h->status = 200;
        contentf(h, f);
        fclose(f);
        magic_close(myt);
    }
    else
    {
        char buffer[10240];
        h->content_type = "text/plain";
        snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", msg.get("Datei nicht gefunden.txt").c_str());
        buffer[sizeof(buffer) -1] = '\0';
        h->extra_header.push_back(buffer);

        h->status = 200;
    }
}

class HttpFilesystemStreamThread : public ServerSocket
{
public:
    pthread_t id;
    pthread_mutex_t mutex;

    int client;
    int pid;

    HttpFilesystemStreamThread(int client, int pid);
    ~HttpFilesystemStreamThread() {};

    void wdebug(int debuglevel, const char *str, int length )
    {
        msg.wdebug(debuglevel, str, length);
    }
};

void *HttpFilesystemStreamThreadLoop(void *param)
{

#if defined(__MINGW32__) || defined(__CYGWIN__)
    _setmbcp(_MB_CP_ANSI);
#endif

    HttpFilesystemStreamThread *p = (HttpFilesystemStreamThread*)param;
    char str[1024];

    listen(p->getSocket(), 1);
    pthread_mutex_unlock(&(p->mutex));

    struct pollfd pollfds[1];

    pollfds[0].fd = p->getSocket();
    pollfds[0].events = POLLIN | POLLPRI;
    if ( poll(pollfds, 1, 1000) > 0 )
    {
        struct sockaddr_storage caddr;
        int rd = p->accept(caddr);
        char buffer[10241], *c;
        int line,len;

        line = 0;
        while ( line < 2 && (len = ::read( rd, buffer, sizeof(buffer) - 1) ) > 0 )
        {
            for ( c=buffer; len != 0; c++ )
            {
                if ( *c != '\r' && *c != '\n') line = 0;
                if ( *c == '\n' ) line++;
                len--;
                if ( line == 2 ) break;
            }
        }

        sprintf(str, "HTTP/1.1 200 OK\r\n");
        ::write(p->client, str, strlen(str));
        sprintf(str, "Content-Type: video/mp4\r\n");
        ::write(p->client, str, strlen(str));
        sprintf(str, "Transfer-Encoding: chunked\r\n\r\n");
        ::write(p->client, str, strlen(str));
        ::write(p->client, c++, len);

        while ( (len = ::read( rd, buffer, sizeof(buffer) - 1) ) > 0 )
            if ( ::write(p->client, buffer, len) < 0 ) break;

        close(p->client);
        close(rd);
    }
    else
    {
        sprintf(str, "HTTP/1.1 404 Not Found\r\n");
        ::write(p->client, str, strlen(str));
        sprintf(str, "Server: M Nelson Embedded Http Server 1.0\r\n");
        ::write(p->client, str, strlen(str));
		sprintf(str, "Connection: Close\r\n");
        ::write(p->client, str, strlen(str));
   	    sprintf(str, "Content-Length: 0\r\n\r\n");
        ::write(p->client, str, strlen(str));

        close(p->client);
    }

    close(p->getSocket());

    pthread_mutex_lock(&(p->mutex));
    pthread_mutex_unlock(&(p->mutex));

    int status;
    waitpid(p->pid, &status, 0);
    //sprintf(str, "waitready %d\n", WEXITSTATUS(status));
    //p->wdebug(1, str, strlen(str));

    delete p;
    pthread_exit(NULL);
    return NULL;
}

HttpFilesystemStreamThread::HttpFilesystemStreamThread(int client, int pid)
                           :ServerSocket(0)
{
    this->client = client;
    this->pid = pid;

    pthread_mutex_init(&this->mutex,NULL);
    pthread_mutex_lock(&mutex);

    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&(this->id), &attr, HttpFilesystemStreamThreadLoop, (void *)this);
    pthread_attr_destroy(&attr);
}


void HttpFilesystem::stream(HttpHeader *h)
{
    std::string name;
    struct stat buf;
    int pid;

    name = getDir(h->vars["dirInput.old"]) + "/" + h->vars["filenameInput.old"];
    if ( name == "" || stat(name.c_str(), &buf) != 0 )
    {
        char buffer[10240];
        h->content_type = "text/plain";
        snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", msg.get("Datei nicht gefunden.txt").c_str());
        buffer[sizeof(buffer) -1] = '\0';
        h->extra_header.push_back(buffer);

        h->status = 200;
        return;
    }

    HttpFilesystemStreamThread *s = new HttpFilesystemStreamThread(h->client, 0);
    pthread_mutex_lock(&(s->mutex));

    pid = vfork();
    if ( pid < 0 )
    {
        msg.perror(E_FORK,"vfork konnte nicht durchgeführt werden");
        return;
    }
    if ( pid == 0 )
    {
        char addr[128];
        char **argv = new char*[32];
        unsigned int i;
        int lfile;

        sprintf(addr, "http://localhost:%u", s->getSocketnum());

        i = 0;
        argv[i++] = (char *)"/usr/bin/ffmpeg";
        argv[i++] = (char *)"-i";
        argv[i++] = (char *)name.c_str();
        argv[i++] = (char *)"-vcodec";
        argv[i++] = (char *)"libx264";
        argv[i++] = (char *)"-preset";
        argv[i++] = (char *)"ultrafast";
        argv[i++] = (char *)"-metadata";
        argv[i++] = (char *)"mime-type=\"video/mp4\"";
        argv[i++] = (char *)"-f";
        argv[i++] = (char *)"mp4";
        argv[i++] = (char *)"-movflags";
        argv[i++] = (char *)"frag_keyframe+empty_moov";
        argv[i++] = addr;
        argv[i] = NULL;

        lfile = open("/tmp/stream.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );
        dup2(lfile, 2);

        close(lfile);

        execve(argv[0], argv, environ);
        _exit(-1);
    }
    else
    {
        s->pid = pid;
        h->client = -1;
        pthread_mutex_unlock(&(s->mutex));
    }

    return;

}

void HttpFilesystem::mkicon(HttpHeader *h)
{
    std::string root  = h->vars["rootInput.old"];
    std::string dir   = h->vars["dirInput.old"];
    std::string  x    = h->vars["x"];
    std::string  y    = h->vars["y"];
    std::string  name = h->vars["name"];

    std::string cname,file;
    struct stat statbuf;

    struct timespec mod;
    mod.tv_sec = mod.tv_nsec = -1;

    cname = this->cacheroot + DIRSEP + root + DIRSEP + dir + DIRSEP + x + "_" + y;

    if ( lstat((getDir(h->vars["dirInput.old"]) + "/" + h->vars["name"]).c_str(), &statbuf) == 0 )
    {
       mod.tv_sec = statbuf.st_mtime;
       mod.tv_nsec = statbuf.st_mtime;
    }

    file = cname + DIRSEP + name + "." + std::to_string(statbuf.st_size);
    if ( lstat(file.c_str(), &statbuf) == 0 )
    {
        if ( mod.tv_sec == statbuf.st_mtime )
        {
            FILE *c = fopen(file.c_str(), "rb");
            if ( c != NULL )
            {
                h->status = 200;
                contentf(h, c);
                fclose(c);
                h->translate = 1;
                return;
            }
        }
    }

    (*(h->vars.p_getExtraVars()))["cpath"] = cname;
    (*(h->vars.p_getExtraVars()))["realpath"] = this->getDir(h->vars["dirInput.old"]);
    (*(h->vars.p_getExtraVars()))["filesize"] = std::to_string(statbuf.st_size);
    findfile(h);
}


