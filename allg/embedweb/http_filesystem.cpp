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

//#if defined(__MINGW32__) || defined(__CYGWIN__)
//#else
//#endif

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
        root =  root + "/";


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

    if ( name != "" )  dir = dir + "/" + name;
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
    DIR *dp;
    struct dirent dirp;
    struct dirent *result;

    int pointdir = ( h->vars["pointdirInput.old"] != "" );

    files.clear();
    dirs.clear();

    if ( getDir(h) == "" )
        return;

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
    if ( dir != "" && dir[dir.length() -1] != '/' )
        dir = dir + "/";

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

    if ( ::mkdir((dir + "/" + name).c_str(), 0777 ) != 0 )
    {
        char buf[1024];
        char *str = strerror_r(errno, buf, sizeof(buf));
        msg.perror(E_CREATEFILE, "Fehler während des Erstellens eines Ordners");
        msg.line("%s", str);
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }

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

    if ( ::rmdir(name.c_str()) != 0 )
    {
        char buf[1024];
        char *str = strerror_r(errno, buf, sizeof(buf));
        msg.perror(E_DELFILE, "Fehler während des Löschen eines Ordners");
        msg.line("%s", str);

        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }

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

    if ( ::rename((path + "/" + oldname).c_str(), (path + "/" + newname).c_str()) != 0 )
    {
        char buf[1024];
        char *str = strerror_r(errno, buf, sizeof(buf));
        msg.perror(E_CREATEFILE, "Fehler während des Umbenennes eines Ordner oder Dateis");
        msg.line("%s", str);
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }

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
                int file2 = open((path + "/" + name).c_str(), O_WRONLY | O_CREAT, 0666 );
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

    if ( ::unlink(name.c_str()) != 0 )
    {
        char buf[1024];
        char *str = strerror_r(errno, buf, sizeof(buf));
        msg.perror(E_DELFILE, "Fehler während des Löschen eines Ordners");
        msg.line("%s", str);

        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>", h->charset.c_str());
        return;
    }

    fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>ok</body>", h->charset.c_str());
}

