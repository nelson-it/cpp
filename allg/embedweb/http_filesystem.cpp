#ifdef PTHREAD
#include <pthread.h>
#endif

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

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <windows.h>

#define DIRSEP   "\\"

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


HttpFilesystem::HttpFilesystem(Http *h, int noadd ) :

    HttpProvider(h),
    search(h, "file", 1),
    msg("HttpFilesystem")
{
    Argument a;

    this->dataroot = std::string(a["EmbedwebHttpDataroot"]);
    if ( this->dataroot[0] != '/' ) this->dataroot = (std::string)(a["projectroot"]) + "/" + this->dataroot;

    this->cacheroot = std::string(a["EmbedwebHttpFileCacheroot"]);
    if ( this->cacheroot[0] != '/' ) this->cacheroot = (std::string)(a["projectroot"]) + "/" + this->cacheroot;

    subprovider["ls.xml"]     = &HttpFilesystem::ls;
    subprovider["mkdir.xml"]  = &HttpFilesystem::mkdir;
    subprovider["rmdir.xml"]  = &HttpFilesystem::rmdir;
    subprovider["mkfile.html"] = &HttpFilesystem::mkfile;
    subprovider["rmfile.xml"] = &HttpFilesystem::rmfile;
    subprovider["mklink.xml"] = &HttpFilesystem::mklink;
    subprovider["mv.xml"] = &HttpFilesystem::mv;
    subprovider["download.html"] = &HttpFilesystem::download;
    subprovider["mk_icon.php"] = &HttpFilesystem::mkicon;

    if ( noadd == 0 )
        h->add_provider(this);
}

HttpFilesystem::~HttpFilesystem()
{
}

int HttpFilesystem::request(HttpHeader *h)
{

    SubProviderMap::iterator i;

    if ( (i = subprovider.find(h->filename)) != subprovider.end() )
    {
        h->status = 200;
        h->content_type = "text/xml";

        (this->*(i->second))(h);
        return 1;
    }

    (*(h->vars.p_getExtraVars()))["root"] = this->getRoot(h);
    (*(h->vars.p_getExtraVars()))["realpath"] = this->getDir(h);

    return search.request(h);

}

std::string HttpFilesystem::getRoot(HttpHeader *h )
{
    std::map<std::string,std::string>::iterator m;
    std::string  root = h->vars["rootInput.old"];

    for (m = h->datapath.begin(); m != h->datapath.end(); ++m )
    {
        msg.pdebug(D_ROOTDIRS, "check %s %s", root.c_str(), m->first.c_str());
        if ( m->first == root ) break;
    }

    if ( m == h->datapath.end() )
    {
        if ( h->user == "admindb" && root == "admin" )
            return this->dataroot;
        return "";
    }

    msg.pdebug(D_ROOTDIRS, "found %s", (this->dataroot + m->second).c_str());
    return this->dataroot + m->second;

}

std::string HttpFilesystem::getDir(HttpHeader *h)
{
    char rpath[PATH_MAX + 1];
    char *resolvpath;
    dir = h->vars["dirInput.old"];
    std::string root = getRoot(h);

    if ( dir != "" && root != "/" )
        root =  root + DIRSEP;


#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::replace( root.begin(), root.end(), '/', '\\');
    std::replace( dir.begin(), dir.end(), '/', '\\');
#endif

    rpath[0] = '\0';
    if ( root == ""
         || ( resolvpath = realpath((root + dir).c_str(), rpath)) == NULL
         || strstr(rpath, root.c_str()) == NULL )
    {
    	msg.pdebug(0, "rpath: %s, root: %s", rpath, root.c_str());
        msg.perror(E_FILENOTFOUND, "Der Ordner <%s> wurde nicht gefunden", (h->vars["rootInput.old"] + ":" + dir).c_str());
        return "";
    }

    path = rpath;
    return rpath;
}

std::string HttpFilesystem::check_path(HttpHeader *h, std::string name, int needname, int errormsg, std::string *result )
{
    getDir(h);
    return check_path(path, name, needname, errormsg, result );
}

std::string HttpFilesystem::check_path(std::string dir, std::string name, int needname, int errormsg, std::string *result )
{

    if ( needname && name == "" )
    {
        msg.perror(E_NEEDNAME, "Der Name der Datei darf nicht leer sein");
        return "";
    }

    if ( dir  != "" && name != "" )  dir = dir + DIRSEP;
    if ( name != "" )  dir = dir + name;

    if ( result != NULL )
        (*result) = dir;

    if ( stat(dir.c_str(), &statbuf) == 0 )
        return dir;

    if ( errormsg )
    {
        std::string str(msg.getSystemerror(errno));
        msg.perror(E_FILENOTFOUND, "Die Datei <%s> wurde nicht gefunden", name.c_str());
        msg.line("%s", str.c_str());
    }

    return "";
}

int HttpFilesystem::quicksort_check(FileData *data1, FileData *data2 )
{
   switch(this->qs_type)
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

void HttpFilesystem::quicksort(std::vector<FileData> &sort, int left, int right)
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
        while ( quicksort_check(&(sort[++i]), x) );
        while ( quicksort_check(x, &(sort[--j])) && j > i );
        if ( i >= j )
            break;
        temp = sort[i];
        sort[i] = sort[j];
        sort[j] = temp;
    }

    temp = sort[i];
    sort[i] = sort[right];
    sort[right] = temp;

    quicksort(sort, left, i-1);
    quicksort(sort, i+1, right);
}

void HttpFilesystem::readdir(HttpHeader *h)
{
    int pointdir = ( h->vars["pointdirInput.old"] != "" );
    files.clear();
    dirs.clear();

    if ( getDir(h) == "" )
        return;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    HANDLE hh;
    WIN32_FIND_DATA d;
    std::string str;

    str = path + "\\*";
    hh = FindFirstFile(str.c_str(), &d);
    if (hh == INVALID_HANDLE_VALUE)
    {
        msg.perror(E_FILENOTFOUND, "Der Ordner <%s> wurde nicht gefunden", ( root + ":" + h->vars["dirInput.old"] ).c_str());
        return;
    }

    do
    {
        if ( ( !pointdir && (std::string(d.cFileName))[0] == '.' )|| (std::string(d.cFileName)) == "." || (std::string(d.cFileName)) == ".." ) continue;

        if ( d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            dirs.insert(d.cFileName);
        else
            files.insert(d.cFileName);
    }
    while (FindNextFile(hh, &d) );
    FindClose(hh);
#else
    DIR *dp;
    struct dirent dirp;
    struct dirent *result;
    std::string dir;

    if((dp  = opendir(path.c_str())) == NULL)
    {
        msg.perror(E_FILENOTFOUND, "Der Ordner <%s> wurde nicht gefunden", ( root + ":" + h->vars["dirInput.old"] ).c_str());
        return;
    }

    dir = path;
    if ( dir != "" && dir.substr(dir.length() - 1) != std::string(DIRSEP) )
        dir = dir + DIRSEP;

    while ((readdir_r(dp, &dirp, &result) == 0 ) && result != NULL )
    {
        if ( ( !pointdir && (std::string(dirp.d_name))[0] == '.' )|| (std::string(dirp.d_name)) == "." || (std::string(dirp.d_name)) == ".." ) continue;
        {
        FileData data;
        data.name = dirp.d_name;
        check_path(root, dir + data.name);
        data.statbuf = statbuf;

        if ( dirp.d_type == DT_DIR )
            dirs.push_back(data);
        else if ( dirp.d_type == DT_REG || dirp.d_type == DT_UNKNOWN )
            files.push_back(data);
        }
    }

    closedir(dp);

    this->qs_type = (FileDataSort)atoi(h->vars["sorttyp"].c_str());
    quicksort( dirs,  0,  dirs.size() - 1);
    quicksort( files, 0, files.size() - 1);

    return;
#endif
}

void HttpFilesystem::ls(HttpHeader *h)
{
    unsigned int i;
    std::string str;

    std::vector<FileData>::iterator is;

    std::string hroot = h->vars["rootInput.old"];
    std::string rootname = h->vars["rootname"];
    std::string idname = h->vars["idname"];

    int onlydir = ( h->vars["noleaf"] != "" );
    int singledir = ( h->vars["singledir"] != "" );

    std::string dir;
    std::string root = getRoot(h);

    if ( dir != "" && root != "/" )
        root =  root + DIRSEP;

    idname = ( idname == "" ) ? "fullname" : idname;
    rootname = ( rootname == "" ) ? "root" : rootname;

    fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result>", h->charset.c_str());

    readdir(h);
    if ( h->error_found )
    {
        fprintf(h->content, "<body>error</body>");
        return;
    }

    fprintf(h->content,"<head>");

    fprintf(h->content,"<d><id>%s</id><typ>2</typ><format></format><name>%s</name><regexp><reg></reg><help></help><mod></mod></regexp></d>\n", "menuid",   "menuid");
    fprintf(h->content,"<d><id>%s</id><typ>2</typ><format></format><name>%s</name><regexp><reg></reg><help></help><mod></mod></regexp></d>\n", "item",   "item");
    fprintf(h->content,"<d><id>%s</id><typ>2</typ><format></format><name>%s</name><regexp><reg></reg><help></help><mod></mod></regexp></d>\n", "action", "action");
    fprintf(h->content,"<d><id>%s</id><typ>2</typ><format></format><name>%s</name><regexp><reg></reg><help></help><mod></mod></regexp></d>\n", "typ",    "typ");
    fprintf(h->content,"<d><id>%s</id><typ>2</typ><format></format><name>%s</name><regexp><reg></reg><help></help><mod></mod></regexp></d>\n", "pos",    "pos");
    fprintf(h->content,"</head><body>");

    dir = this->dir;
    if ( dir != "" && dir.substr(dir.length() - 1) != std::string(DIRSEP) )
        dir = dir + DIRSEP;

    i = 0;
    for ( is= dirs.begin(); is != dirs.end(); ++is )
    {
        char str[1024];
        str[sizeof(str) - 1] =  '\0';
        snprintf(str, sizeof(str) - 1, "setValue({ %s : \"%s\", %s : \"%s\", name : \"%s\", leaf : false, createtime : %ld, modifytime : %ld, accesstime : %ld })", rootname.c_str(), hroot.c_str(), idname.c_str(), ( dir + (*is).name).c_str(), (*is).name.c_str(), (*is).statbuf.st_ctime, (*is).statbuf.st_mtime, (*is).statbuf.st_atime );
        if ( singledir )
            fprintf(h->content,"<r><v>%s</v><v>%s</v><v>%s</v><v>%s</v><v>%d</v></r>", (dir + (*is).name).c_str(), (*is).name.c_str(), str, "leaf", i++ );
        else
            fprintf(h->content,"<r><v>%s</v><v>%s</v><v>%s</v><v>%s</v><v>%d</v></r>", (dir + (*is).name).c_str(), (*is).name.c_str(), "submenu", "", i++ );
    }

    for ( is= files.begin(); !onlydir && is != files.end(); ++is )
    {
        char str[1024];
        str[sizeof(str) - 1] =  '\0';
        snprintf(str, sizeof(str) - 1, "setValue({ %s : \"%s\", %s : \"%s\", name : \"%s\", leaf : true, createtime : %ld, modifytime : %ld, accesstime : %ld })", rootname.c_str(), hroot.c_str(), idname.c_str(), ( dir + (*is).name).c_str(), (*is).name.c_str(), (*is).statbuf.st_ctime, (*is).statbuf.st_mtime, (*is).statbuf.st_atime );
        fprintf(h->content,"<r><v>%s</v><v>%s</v><v>%s</v><v>%s</v><v>%d</v></r>", (dir + (*is).name).c_str(), (*is).name.c_str(), str, "leaf", i++ );
    }

    fprintf(h->content,"</body>");
    return;

}

void HttpFilesystem::mkdir(HttpHeader *h)
{
    std::string dir = check_path(h, "", 0);
    std::string name = h->vars["filenameInput"];

    if ( dir == "" || name == "" )
    {
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }

#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( ! CreateDirectory((dir + DIRSEP + name).c_str(), NULL) )
    {
        msg.perror(E_CREATEFILE, "Fehler während des Erstellens eines Ordners");
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }
#else
    if ( ::mkdir((dir + DIRSEP + name).c_str(), 0777 ) != 0 )
    {
        std::string str = msg.getSystemerror(errno);
        msg.perror(E_CREATEFILE, "Fehler während des Erstellens eines Ordners");
        msg.line("%s", str.c_str());
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }

    mode_t mask;
    mask = umask(0);
    umask(mask);
    chmod((dir + DIRSEP + name).c_str(), (02777 & ~ mask));

#endif
    fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>ok</body>", h->charset.c_str());


}
void HttpFilesystem::rmdir(HttpHeader *h)
{
    std::string name = check_path(h, "", 0);

    if (  name == "" )
    {
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }

#if defined(__MINGW32__) || defined(__CYGWIN__)
    SetFileAttributes(name.c_str(), FILE_ATTRIBUTE_NORMAL);
    if ( ! RemoveDirectory(name.c_str()) )
    {
        msg.perror(E_DELFILE, "Fehler während des Löschen eines Ordners");
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }
#else
    if ( ::rmdir(name.c_str()) != 0 )
    {
        std::string str = msg.getSystemerror(errno);
        msg.perror(E_DELFILE, "Fehler während des Löschen eines Ordners");
        msg.line("%s", str.c_str());

        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }
#endif
    fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>ok</body>", h->charset.c_str());
}

void HttpFilesystem::mv(HttpHeader *h)
{
    std::string oldname = h->vars["filenameInput.old"];
    std::string newname = h->vars["filenameInput"];

    if ( oldname == "" || newname == "" )
    {
        msg.perror(E_NEEDNAME, "Der Name der Datei darf nicht leer sein");
    }

    if ( getDir(h) == "" || oldname == "" || newname == "" )
    {
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }

#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( ! MoveFile((path + DIRSEP + oldname).c_str(), (path + DIRSEP + newname).c_str()) )
     {
         msg.perror(E_CREATEFILE, "Fehler während des Umbenennes eines Ordner oder Datei");
         fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
         return;
     }
#else
   if ( ::rename((path + DIRSEP + oldname).c_str(), (path + DIRSEP + newname).c_str()) != 0 )
    {
        std::string str = msg.getSystemerror(errno);
        msg.perror(E_CREATEFILE, "Fehler während des Umbenennes eines Ordner oder Datei");
        msg.line("%s", str.c_str());
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }
#endif
    fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>ok</body>", h->charset.c_str());


}

void HttpFilesystem::mkfile(HttpHeader *h)
{
    std::string str;

    h->status = 200;
    h->content_type = "text/html";

    str = h->vars.getFile("dataInput");
    if ( str == "" )
    {
        str = msg.get("Keine Daten gefunden");
    }
    else
    {
#if defined(__MINGW32__) || defined(__CYGWIN__)
        std::string name = h->vars["filenameInput"];
        if ( getDir(h) == "" || name == "" )
        {
            str = msg.get("Benötige einen Dateinamen");
        }
        if ( ! CopyFile(str.c_str(), (path + DIRSEP + name).c_str(), FALSE) )
        {
            str = msg.getSystemerror(errno);
        }
        str = "ok";
#else
        int file1;
        if ( ( file1 = open(str.c_str(), O_RDONLY)) < 0 )
        {
            str = msg.getSystemerror(errno);
        }
        else
        {
            std::string name = h->vars["filenameInput"];

            if ( getDir(h) == "" || name == "" )
            {
                str = msg.get("Benötige einen Dateinamen");
            }
            else
            {
                int file2 = open((path + DIRSEP + name).c_str(), O_WRONLY | O_CREAT, 0666 );
                if ( file2 < 0 )
                {
                    str = msg.getSystemerror(errno);
                    close(file1);
                }
                else
                {
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
                    str = "ok";
                }
            }
        }
#endif
    }

    if ( h->vars["script"] != "" )
    {
        fprintf(h->content,"<script type=\"text/javascript\">\n");
        fprintf(h->content,"<!--\n");
        fprintf(h->content,"%s\n", h->vars["script"].c_str());
        fprintf(h->content,"//-->\n");
        fprintf(h->content,"</script>\n");
    }

    fprintf(h->content, "%s", str.c_str());
}

void HttpFilesystem::rmfile(HttpHeader *h)
{
    std::string name = check_path(h, h->vars["filenameInput.old"]);

    if (  name == "" )
    {
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }

#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( ! DeleteFile(name.c_str()) )
    {
        msg.perror(E_DELFILE, "Fehler während des Löschen eines Ordners");
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }
#else
    if ( ::unlink(name.c_str()) != 0 )
    {
        std::string str = msg.getSystemerror(errno);
        msg.perror(E_DELFILE, "Fehler während des Löschen eines Ordners");
        msg.line("%s", str.c_str());

        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }
#endif
    fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>ok</body>", h->charset.c_str());
}

void HttpFilesystem::mklink(HttpHeader *h)
{
    std::string root,dir1,dir2,file1,file2;
    int result;

    fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result>", h->charset.c_str());

    result = ( root = this->getRoot(h) ) == "";

    if ( ! result ) result = ( dir1 = this->check_path(root, h->vars["dirInput.old"], 0) ) == "";
    if ( ! result ) result = ( dir2 = this->check_path(root, h->vars["dirInput"]    , 0) ) == "";

    if ( ! result ) result = ( file1 = this->check_path(dir1, h->vars["filenameInput.old"], 1) ) == "";
    if ( ! result )
    {
        if ( ( result = ( this->check_path(dir2, h->vars["filenameInput"], 1, 0, &file2 ) != "" )) )
        {
            if ( h->vars["overwriteInput"] == "" )
                msg.perror(E_FILEEXISTS, "Datei existiert schon");
            else
                result = 0;
        }
    }


#if defined(__MINGW32__) || defined(__CYGWIN__)

    if ( ! result )
    {
        if ( h->vars["symlink"] != "" )
            result = ! CreateHardLink(file2.c_str(), file1.c_str(), NULL);
        else
            result = ! CreateSymbolicLink(file2.c_str(), file1.c_str(), NULL);
    }

#else

    if ( ! result )
    {
        if ( h->vars["symlink"] != "" )
            result = symlink(file1.c_str(), file2.c_str());
        else
            result = link(file1.c_str(), file2.c_str());
    }

#endif

    if ( result )
    {
        msg.perror(E_CREATELINK,"Kann hardlink <%s -> %s> nicht erstellen", (h->vars["dirInput.old"] + "/" + h->vars["filenameInput.old"]).c_str(), (h->vars["dirInput"] + "/" + h->vars["filenameInput"]).c_str());
        fprintf(h->content, "<body>error</body>");
        return;
    }
    fprintf(h->content, "<body>ok</body>");
}

void HttpFilesystem::download(HttpHeader *h)
{
    std::string name;
    FILE *f;

    name = check_path(h, h->vars["filenameInput.old"], 1);
    if ( name != "" && ( f = fopen(name.c_str(), "r")) != NULL )
    {
        char buffer[10240];
        h->content_type = "application/octet-stream";
        snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", h->vars.url_decode(h->vars["filenameInput.old"].c_str()).c_str());
        buffer[sizeof(buffer) -1] = '\0';
        h->extra_header.push_back(buffer);

        h->status = 200;
        fclose(h->content);
        h->content = f;
        fseek(h->content, 0, SEEK_END);
    }
    else
    {
        h->status = 404;
    }
}

void HttpFilesystem::mkicon(HttpHeader *h)
{
    std::string root  = h->vars["rootInput.old"];
    std::string dir   = h->vars["dirInput.old"];
    std::string  x    = h->vars["x"];
    std::string  y    = h->vars["y"];
    std::string  name = h->vars["name"];

    std::string cname,file;

    struct timespec mod;
    mod.tv_sec = mod.tv_nsec = -1;

    cname = this->cacheroot + DIRSEP + root + DIRSEP + dir + DIRSEP + x + "_" + y;

    if ( check_path(this->getDir(h), name, 1, 0) != "" )
    {
       mod.tv_sec = this->statbuf.st_mtime;
       mod.tv_nsec = this->statbuf.st_mtime;
    }

    if ( ( file = check_path(cname, name + "." + std::to_string(this->statbuf.st_size), 1, 0)) != "" )
    {
        if ( mod.tv_sec == this->statbuf.st_mtime )
        {
            FILE *c = fopen(file.c_str(), "rb");
            if ( c != NULL )
            {
                h->status = 200;
                fclose(h->content);
                h->content = c;
                h->translate = 1;
                fseek(c, 0, SEEK_END);
                return;
            }
        }
    }

    (*(h->vars.p_getExtraVars()))["cpath"] = cname;
    (*(h->vars.p_getExtraVars()))["realpath"] = this->getDir(h);
    (*(h->vars.p_getExtraVars()))["filesize"] = std::to_string(this->statbuf.st_size);
    search.request(h);

}


