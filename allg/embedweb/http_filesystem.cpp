#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

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
HttpProvider(h), msg("HttpFilesystem")
{
    subprovider["ls.xml"]     = &HttpFilesystem::ls;
    subprovider["mkdir.xml"]  = &HttpFilesystem::mkdir;
    subprovider["rmdir.xml"]  = &HttpFilesystem::rmdir;
    subprovider["mkfile.html"] = &HttpFilesystem::mkfile;
    subprovider["rmfile.xml"] = &HttpFilesystem::rmfile;
    subprovider["mv.xml"] = &HttpFilesystem::mv;

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
    return 0;

}

std::string HttpFilesystem::getRoot(HttpHeader *h )
{
    std::map<std::string,std::string>::iterator m;
    std::string  root = h->vars["rootInput.old"];

    for (m = h->datapath.begin(); m != h->datapath.end(); ++m )
        if ( m->first == root ) break;

    if ( m == h->datapath.end() ) return "";

    return m->second;

}

std::string HttpFilesystem::getDir(HttpHeader *h)
{
    char rpath[PATH_MAX + 1];
    dir = h->vars["dirInput.old"];
    std::string root = getRoot(h);

    if ( dir != "" )
        root =  root + DIRSEP;


    if ( root == ""
         || realpath((root + dir).c_str(), rpath) == NULL
         || strstr(rpath, root.c_str()) == NULL )
    {
        msg.perror(E_FILENOTFOUND, "Der Ordner <%s> wurde nicht gefunden", (h->vars["rootInput.old"] + ":" + dir).c_str());
        return "";
    }

    path = rpath;
    return rpath;
}

std::string HttpFilesystem::check_path(HttpHeader *h, std::string name, int needname, int errormsg )
{
    getDir(h);
    return check_path(path, name, needname, errormsg );
}

std::string HttpFilesystem::check_path(std::string dir, std::string name, int needname, int errormsg )
{
    struct stat buf;

    if ( needname && name == "" )
    {
        msg.perror(E_NEEDNAME, "Der Name der Datei darf nicht leer sein");
        return "";
    }

    if ( name != "" )  dir = dir + DIRSEP + name;
    if ( stat(dir.c_str(), &buf) == 0 )
        return dir;

    if ( errormsg )
    {
        std::string str(msg.getSystemerror(errno));
        msg.perror(E_FILENOTFOUND, "Die Datei <%s> wurde nicht gefunden", name.c_str());
        msg.line("%s", str.c_str());
    }

    return "";
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

    if((dp  = opendir(path.c_str())) == NULL)
    {
        msg.perror(E_FILENOTFOUND, "Der Ordner <%s> wurde nicht gefunden", ( root + ":" + h->vars["dirInput.old"] ).c_str());
        return;
    }

    while ((readdir_r(dp,&dirp,&result) == 0 ) && result != NULL )
    {
        if ( ( !pointdir && (std::string(dirp.d_name))[0] == '.' )|| (std::string(dirp.d_name)) == "." || (std::string(dirp.d_name)) == ".." ) continue;

        if ( dirp.d_type == DT_DIR )
            dirs.insert(dirp.d_name);
        else if ( dirp.d_type == DT_REG || dirp.d_type == DT_UNKNOWN )
            files.insert(dirp.d_name);
    }
    closedir(dp);
    return;
#endif
}

void HttpFilesystem::ls(HttpHeader *h)
{
    unsigned int i;
    std::string str;

    std::set<std::string>::iterator is;

    std::string rootname = h->vars["rootname"];
    std::string idname = h->vars["idname"];

    int onlydir = ( h->vars["noleaf"] != "" );
    int singledir = ( h->vars["singledir"] != "" );

    std::string dir;

    idname = ( idname == "" ) ? "name" : idname;
    rootname = ( rootname == "" ) ? "root" : rootname;

    fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result>", h->charset.c_str());

    readdir(h);
    if ( h->error_found )
    {
        fprintf(h->content, "<body>error</body>");
        return;
    }

    fprintf(h->content,"<head>");

    fprintf(h->content,"<d><id>%s</id><typ>2</typ><format></format><name>%s</name><regexp><reg></reg><help></help></regexp></d>\n", "menuid",   "menuid");
    fprintf(h->content,"<d><id>%s</id><typ>2</typ><format></format><name>%s</name><regexp><reg></reg><help></help></regexp></d>\n", "item",   "item");
    fprintf(h->content,"<d><id>%s</id><typ>2</typ><format></format><name>%s</name><regexp><reg></reg><help></help></regexp></d>\n", "action", "action");
    fprintf(h->content,"<d><id>%s</id><typ>2</typ><format></format><name>%s</name><regexp><reg></reg><help></help></regexp></d>\n", "typ",    "typ");
    fprintf(h->content,"<d><id>%s</id><typ>2</typ><format></format><name>%s</name><regexp><reg></reg><help></help></regexp></d>\n", "pos",    "pos");
    fprintf(h->content,"</head><body>");

    dir = this->dir;
    if ( dir != "" && dir.substr(dir.length() - 1) != std::string(DIRSEP) )
        dir = dir + DIRSEP;

    i = 0;
    for ( is= dirs.begin(); is != dirs.end(); ++is )
    {
        if ( singledir )
            fprintf(h->content,"<r><v>%s</v><v>%s</v><v>%s</v><v>%s</v><v>%d</v></r>", (dir + (*is)).c_str(), (*is).c_str(), ("setValue( \"" + rootname + " : '" + root + "', " + idname + ": '" + dir + (*is) + "'\")").c_str(), "leaf", i++ );
        else
            fprintf(h->content,"<r><v>%s</v><v>%s</v><v>%s</v><v>%s</v><v>%d</v></r>", (dir + (*is)).c_str(), (*is).c_str(), "submenu", "", i++ );
    }

    for ( is= files.begin(); !onlydir && is != files.end(); ++is )
        fprintf(h->content,"<r><v>%s</v><v>%s</v><v>%s</v><v>%s</v><v>%d</v></r>", (dir + (*is)).c_str(), (*is).c_str(), ("setValue( \"" + rootname + " : '" + root + "'," +  idname + ": '" + dir + (*is) + "'\")").c_str(), "leaf", i++ );

    fprintf(h->content,"</body>");
    return;

}

void HttpFilesystem::mkdir(HttpHeader *h)
{
    std::string dir = check_path(h, h->vars["filenameInput.old"], 0);
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
#endif
    fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>ok</body>", h->charset.c_str());


}
void HttpFilesystem::rmdir(HttpHeader *h)
{
    std::string name = check_path(h, h->vars["filenameInput.old"]);

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
         msg.perror(E_CREATEFILE, "Fehler während des Umbenennes eines Ordner oder Dateis");
         fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
         return;
     }
#else
   if ( ::rename((path + DIRSEP + oldname).c_str(), (path + DIRSEP + newname).c_str()) != 0 )
    {
        std::string str = msg.getSystemerror(errno);
        msg.perror(E_CREATEFILE, "Fehler während des Umbenennes eines Ordner oder Dateis");
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

