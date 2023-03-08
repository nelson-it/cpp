#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <map>

#include <utils/tostring.h>
#include <utils/process.h>
#include <utils/tmpfile.h>
#include <argument/argument.h>

#include "dbquery.h"
#include "dbhttputils_repository.h"

#if defined(__MINGW32__) || defined(__CYGWIN__)
#define DIRSEP   "\\"
#else
#define DIRSEP   "/"
#endif

DbHttpUtilsRepository::DbHttpUtilsRepository(DbHttp *h)
                      :HttpFilesystem(h, 1),
                       DbHttpProvider(h),
                       msg("DbHttpUtilsRepository"),
                       query(h, 1),
                       table(h, 1)
{
    Argument a;
    subprovider["download.dat"]     = &DbHttpUtilsRepository::download;
    subprovider["downloadall.html"] = &DbHttpUtilsRepository::downall;

    subprovider["insert.json"]   = &DbHttpUtilsRepository::insert_json;
    subprovider["modify.json"]   = &DbHttpUtilsRepository::modify;
    subprovider["delete.json"]   = &DbHttpUtilsRepository::del;
    subprovider["data.json"]     = &DbHttpUtilsRepository::data;

    subprovider["ls.json"]       = &DbHttpUtilsRepository::ls;
    subprovider["addfile.json"]  = &DbHttpUtilsRepository::addfile;
    subprovider["mkfile.json"]   = &DbHttpUtilsRepository::mkfile;
    subprovider["rmfile.json"]   = &DbHttpUtilsRepository::rmfile;
    subprovider["mv.json"]       = &DbHttpUtilsRepository::mv;
    subprovider["mkdir.json"]    = &DbHttpUtilsRepository::mkdir;
    subprovider["rmdir.json"]    = &DbHttpUtilsRepository::rmdir;

    subprovider["commit.json"]   = &DbHttpUtilsRepository::commit;
    subprovider["dblog.json"]    = &DbHttpUtilsRepository::dblog;

    gitcmd = std::string(a["gitcmd"]);

    h->add_provider(this);
}

DbHttpUtilsRepository::~DbHttpUtilsRepository()
{
}

int DbHttpUtilsRepository::request(Database *db, HttpHeader *h)
{
#if defined(__MINGW32__) || defined(__CYGWIN__)
    h->status = 200;
    h->content_type = "text/json";
    msg.pwarning(10000, "Repository in Windows nicht möglich");
    DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
    return 1;
#endif

    SubProviderMap::iterator i;
    if ( ( i = subprovider.find(h->filename)) != subprovider.end() )
    {
        h->status = 200;

        read_name(db, h);

        this->rootpath = getRoot(h);
        if ( this->rootpath == "" )
        {
            msg.perror(E_ROOT, "Wurzel unbekannt");
            DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
            return 1;
        }

        if ( h->vars["filenameInput"].find(DIRSEP) != std::string::npos || h->vars["filenameInput.old"].find(DIRSEP) != std::string::npos )
        {
            msg.perror(E_WRONGNAME, "Dateinamen sind im falschen Format");
            DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
            return 1;
        }

        (this->*(i->second))(db, h);

        if (h->vars["sqlend"] != "")
            db->p_getConnect()->end();

        return 1;
    }

    return 0;

}
int DbHttpUtilsRepository::exec(const CsList *cmd, const char *workdir)
{
    char buffer[1025];
    int anzahl;
    int retval;
    CsList cc = *cmd;

    execlog = "";
    Process p(0);
    p.start(*cmd, "pipe", workdir);
    while( ( anzahl = p.read(buffer, sizeof(buffer) - 1)) != 0 )
    {
        if ( anzahl > 0 )
        {
            buffer[anzahl] = '\0';
            execlog += buffer;
        }
        else if ( anzahl < 0 && errno != EAGAIN ) break;
    }

    retval = p.getStatus();
    return retval;
}

void DbHttpUtilsRepository::read_name(Database *db, HttpHeader *h)
{
    DbConnect::ResultMat *r;
    DbTable::ValueMap where;
    DbTable *tab;
    CsList cols("root,name");

    tab = db->p_getTable("mne_repository", "repository");

    where["repositoryid"] = h->vars["repositoryidInput.old"];
    r = tab->select(&cols, &where);
    tab->end();
    db->release(tab);

    if ( ! r->empty() )
    {
        msg.pdebug(1, "found");
        (*h->vars.p_getVars())["rootInput.old"] = (char*)((*r)[0][0]);
        (*h->vars.p_getVars())["nameInput.old"] = (char*)((*r)[0][1]);
    }

    repository = h->vars["nameInput.old"];
}

std::string DbHttpUtilsRepository::getRepositoryRoot()
{
    if ( repository == "")
    {
        msg.perror(E_REPOSITORY, "Der Aktenordner muss einen Namen haben");
        return "";
    }

    return rootpath + repository + DIRSEP;
}

std::string DbHttpUtilsRepository::getDir(std::string dir, int errormsg )
{
    return HttpFilesystem::getDir(repository + DIRSEP + dir, errormsg);
}

int DbHttpUtilsRepository::insert (Database *db, HttpHeader *h)
{
    std::string str;
    std::string val;
    std::string root;
    std::string templ;
    CsList cmd;
    int result;

    DbTable::ColumnMap::iterator ci;
    DbTable::ValueMap vals;
    DbTable::ValueMap orig_vals;
    DbTable::ValueMap where;
    DbTable::ValueMap::iterator wi;

    struct stat statbuf;

    (*h->vars.p_getVars())["rootInput.old"] = h->vars["rootInput"];
    if ( (root = HttpFilesystem::getRoot(h)) == "" )
    {
        msg.perror(E_MKREPOSITORY,"Der Aktenschrank <%s> existiert nicht", h->vars["rootInput"].c_str());
        return -1;
    }

    if ( lstat((root + DIRSEP + h->vars["nameInput"]).c_str(), &statbuf) == 0 )
    {
        msg.pwarning(E_MKREPOSITORY,"Der Aktenordner <%s> existiert schon", h->vars["nameInput"].c_str());
    }
    else
    {
        cmd.add(gitcmd.c_str());
        cmd.add("init");
        cmd.add("--quiet");
        cmd.add("--shared=group");
        cmd.add(h->vars["nameInput"]);

        Process p;
        p.start(cmd, NULL, root.c_str());
        result = p.wait();

        if ( result != 0 )
        {
            msg.perror(E_MKREPOSITORY,"Fehler während des Erzeugens eines Aktenordners");
            return -1;
        }

        mode_t mask = umask(0);
        umask(mask);
        chmod( (root + DIRSEP + h->vars["nameInput"]).c_str(), (02777 & ~ mask) );

        templ = msg.get("RepositoryVorlage");
        if ( lstat((root + DIRSEP + templ).c_str(), &statbuf) == 0 && h->vars["nameInput"] != templ && templ != "" )
        {
            cmd.setString("");
            cmd.add("sh");
            cmd.add("-c");
            cmd.add("( cd \"" + templ + "\"; tar -cf - --exclude .git .) | ( cd \"" + h->vars["nameInput"] + "\"; tar -xf - )");

            p.start(cmd, NULL, root.c_str());
            result = p.wait();

            if ( result != 0 )
            {
                msg.perror(E_MKREPOSITORY,"Fehler während des Erzeugens eines Aktenordners");
                return -1;
            }
        }
        else
        {
#if defined(__MINGW32__) || defined(__CYGWIN__)
            int file = open((root + DIRSEP + h->vars["nameInput"] + DIRSEP + ".gitignore").c_str(), O_WRONLY | O_CREAT, 0666 );
#else
            int file = open((root + DIRSEP + h->vars["nameInput"]+ DIRSEP + ".gitignore").c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0666 );
#endif
            write(file, (std::string("# ") + h->vars["nameInput"] + "\n").c_str(), h->vars["nameInput"].length() + 3);
            close(file);
        }

        cmd.setString("");
        cmd.add(gitcmd.c_str());
        cmd.add("add");
        cmd.add(".");

        p.start(cmd, NULL, ( root + DIRSEP + h->vars["nameInput"]).c_str());
        result = p.wait();

        if ( result != 0 )
        {
            msg.perror(E_MKREPOSITORY,"Fehler während des Erzeugens eines Aktenordners");
            return -1;
        }

        cmd.setString("");
        cmd.add(gitcmd.c_str());
        cmd.add("config");
        cmd.add("user.name");
        cmd.add("Open Source Erp");

        p.start(cmd, NULL, ( root + DIRSEP + h->vars["nameInput"]).c_str());
        result = p.wait();

        if ( result != 0 )
        {
            msg.perror(E_MKREPOSITORY,"Fehler während des Erzeugens eines Aktenordners");
            return -1;
        }

        cmd.setString("");
        cmd.add(gitcmd.c_str());
        cmd.add("config");
        cmd.add("user.email");
        cmd.add("erp@local");

        p.start(cmd, NULL, ( root + DIRSEP + h->vars["nameInput"]).c_str());
        result = p.wait();

        if ( result != 0 )
        {
            msg.perror(E_MKREPOSITORY,"Fehler während des Erzeugens eines Aktenordners");
            return -1;
        }

        DbHttpAnalyse::Client::Userprefs userprefs = DbHttpProvider::http->getUserprefs();
        cmd.setString("");
        cmd.add(gitcmd.c_str());
        cmd.add("commit");
        cmd.add("-m");
        cmd.add(msg.get("Initialversion aus Vorlage"));
        cmd.add("--author=\"" + userprefs["fullname"] + " <" + userprefs["email"] + ">\"" );

        p.start(cmd, NULL, ( root + DIRSEP + h->vars["nameInput"]).c_str());
        result = p.wait();

        if ( result != 0 )
        {
            msg.perror(E_MKREPOSITORY,"Fehler während des Erzeugens eines Aktenordners");
            return -1;
        }
    }

    (*h->vars.p_getVars()).erase("rootInput.old");
    (*h->vars.p_getVars()).erase("nameInput.old");

    return 0;

}

void DbHttpUtilsRepository::insert_json (Database *db, HttpHeader *h)
{
    h->status = 200;
    h->content_type = "text/json";
    if ( this->insert(db, h) != 0 )
        DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
    else
        table.insert_json(db, h);

}

void DbHttpUtilsRepository::modify (Database *db, HttpHeader *h)
{
    std::string name;
    CsList cmd;
    int result = 0;
    struct stat statbuf;

    h->status = 200;
    h->content_type = "text/json";

    name = h->vars["nameInput.old"];

    if ( lstat((rootpath + DIRSEP + name).c_str(), &statbuf) != 0 )
    {
        msg.perror(E_MKREPOSITORY,"Der Aktenordner <%s> existiert nicht", (root + ":" + name).c_str());
        result = -1;
    }

    if ( name == "" || h->vars["nameInput"] == "" )
    {
        msg.perror(E_MKREPOSITORY,"Die Aktenordner müssen eine Namen haben");
        result = -1;
    }

    if ( result == 0 && name != h->vars["nameInput"] )
    {
        cmd.add("mv");
        cmd.add(name);
        cmd.add(h->vars["nameInput"]);

        Process p;
        p.start(cmd, NULL, rootpath.c_str());
        if ( p.wait() != 0 )
        {
            std::string str = msg.getSystemerror(errno);
            msg.perror(E_MKREPOSITORY,"Fehler während des Umbenennen eines Aktenordners");
            msg.line("%s", str.c_str());
            result = -1;
        }
    }

    (*h->vars.p_getVars()).erase("rootInput.old");
    (*h->vars.p_getVars()).erase("nameInput.old");

    DbHttpProvider::del_content(h);
    if ( result == 0 )
        table.modify_json(db, h);
    else
        DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
}

void DbHttpUtilsRepository::del (Database *db, HttpHeader *h)
{
    time_t rawtime;
    struct tm * timeinfo;
    char buffer [80];
    struct stat statbuf;

    time (&rawtime);
    timeinfo = gmtime (&rawtime);

    strftime (buffer,80,"%Y%m%d%H%M%S",timeinfo);

    std::string name = getRepositoryRoot();

    h->status = 200;
    h->content_type = "text/json";

    if ( lstat(name.c_str(), &statbuf) != 0 )
    {
        msg.pwarning(E_DELREPOSITORY,"Der Aktenordner <%s> existiert nicht", h->vars["nameInput.old"].c_str());
    }
#if defined(__MINGW32__) || defined(__CYGWIN__)
    else
    {
        SetFileAttributes(name.c_str(), FILE_ATTRIBUTE_NORMAL);
        if ( ! MoveFile(( name.c_str(), ( rootpath + DIRSEP + ".trash" + DIRSEP + h->vars["nameInput.old"] + "_" + buffer).c_str()) )
        {
            std::string str = msg.getSystemerror(GetLastError());
            msg.perror(E_DELREPOSITORY,"Fehler während des Löschens eines Aktenordners");
            msg.line("%s", str.c_str());
        }
    }
#else
    else if ( rename( name.c_str(), ( rootpath + DIRSEP + ".trash" + DIRSEP + h->vars["nameInput.old"] + "_" + buffer).c_str()) != 0 )
    {
        std::string str = msg.getSystemerror(errno);
        msg.perror(E_DELREPOSITORY,"Fehler während des Löschens eines Aktenordners");
        msg.line("%s", str.c_str());
    }
#endif
    (*h->vars.p_getVars()).erase("rootInput.old");
    (*h->vars.p_getVars()).erase("nameInput.old");

    if ( ! h->error_found )
    {
        (*h->vars.p_getVars())["table"] = "filedata";
        table.delete_json(db,h);

        (*h->vars.p_getVars())["table"] = "fileinterests";
        table.delete_json(db,h);

        DbHttpProvider::del_content(h);
        (*h->vars.p_getVars())["table"] = "repository";
        table.delete_json(db,h);
    }
    else
    {
        DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
    }

}

void DbHttpUtilsRepository::dbdata_checkdir(std::string dir)
{
    std::vector<FileData>::iterator is;
    std::vector<FileData> dirs;
    int f;
    struct stat statbuf;

    std::string file = ( dir + DIRSEP + ".gitignore");
    if ( lstat((rootpath + DIRSEP + file).c_str(), &statbuf) == 0 )
    {
#if defined(__MINGW32__) || defined(__CYGWIN__)
        f = open((rootpath + DIRSEP + file).c_str(), O_WRONLY | O_CREAT, 0666 );
#else
        f = open((rootpath + DIRSEP + file).c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0666 );
#endif
        write(f, (std::string("# ") + file + "\n").c_str(), file.length() + 3);
        close(f);

        HttpFilesystem::readdir(dir , 0 );
        dirs = this->dirs;
        for ( is = dirs.begin(); is != dirs.end(); ++is )
            dbdata_checkdir( (*is).name);
    }
}

void DbHttpUtilsRepository::dbdata_update ( Database *db, HttpHeader *h)
{
    DbTable *tab;
    DbTable::ValueMap where,vals;
    DbConnect::ResultMat *r;
    std::string repository;
    struct stat statbuf;

    std::string schema;
    std::string table;

    if ( h->vars["nameInput.old"] ==  "" ) return;

    schema = h->vars["schema"];
    table = h->vars["table"];
    tab = db->p_getTable(schema, table);

    vals["repositoryid"] = "################";
    where["root"] = vals["root"] = root;
    where["name"] = vals["name"] = h->vars["nameInput.old"];

    r = tab->select(&vals, &where);
    if (r->empty() )
        tab->insert(&vals);
    db->release(tab);

    repository = getRepositoryRoot();
    if ( lstat((repository + ".git").c_str(), &statbuf) != 0 )
    {
        CsList cmd;
        int result;

        dbdata_checkdir(repository);

        cmd.add(gitcmd.c_str());
        cmd.add("init");
        cmd.add("--quiet");
        cmd.add("--shared=group");
        cmd.add(h->vars["nameInput.old"]);

        Process p;
        p.start(cmd, NULL, rootpath.c_str());
        result = p.wait();

        if ( result != 0 )
            msg.perror(E_MKREPOSITORY,"Fehler während des Erzeugens der Versionsverwaltung");

        cmd.clear();
        cmd.add(gitcmd.c_str());
        cmd.add("add");
        cmd.add(".");

        p.start(cmd, NULL, repository.c_str());
        result = p.wait();

        if ( result != 0 )
        {
            msg.perror(E_MKREPOSITORY,"Fehler während des Hinzufügens der Ignoredateien der Versionsverwaltung");
            return;
        }
        (*h->vars.p_getVars())["autocommitInput"] = "1";
        (*h->vars.p_getVars())["commitmessageInput"] = "Initialversion";
        data_commit(db, h);
    }
}

void DbHttpUtilsRepository::data(Database *db, HttpHeader *h)
{
    dbdata_update(db,h);
    query.data_json(db, h);
}

void DbHttpUtilsRepository::read_status( std::string dir, std::map<std::string, std::string> &status)
{
    unsigned int i;
    std::string str;
    CsList cmd;
    int result;
    std::vector<FileData>::iterator is;

    cmd.add(gitcmd.c_str());
    cmd.add("status");
    cmd.add("-s");
    cmd.add(".");

    result = exec(&cmd, dir.c_str());

    if ( result != 0 )
    {
        msg.perror(E_MKREPOSITORY,"Fehler während des Listens eines Ordners");
        msg.line("%s", execlog.c_str());
        return;
    }

    CsList s(execlog, '\n', 1);

    for ( i = 0; i<s.size(); ++i)
    {
        std::string name = s[i].substr(3);
        std::string st = s[i].substr(0,2);
        int deleted = (s[i].substr(1,1) == "D" );

        std::string::size_type n;

        if ( (n = name.find(" -> ")) != std::string::npos ) name = name.substr(n + 4);
        if ( name[0] == '"') name = name.substr(1,name.find('"', 1) - 1);
        if ( name[name.size() - 1] == '/') name = name.substr(0,name.size() - 1);

        if ( st == "??" ) st = "Y";
        st.erase( std::remove_if( st.begin(), st.end(), ::isspace ), st.end() );

        if ( name == "." )
        {
            for ( is = files.begin(); is != files.end(); ++is) status.insert(std::make_pair((*is).name, ( deleted ) ? "D" : st));
        }
        else
        {
        status.insert(std::make_pair(name, ( deleted ) ? "D" : st));
        }

        if ( deleted && name.find('/') == std::string::npos )
        {
            FileData d;
            d.name = name;
            files.push_back(d);
        }

        if ( ( n = name.find('/')) != std::string::npos )
        {
            name = name.substr(0, n);
            for ( is = dirs.begin(); is != dirs.end(); ++is) if ( (*is).name == name ) break;
            if ( is != dirs.end() )
                status.insert(std::make_pair(name, "M"));
        }
    }
    return;
}

void DbHttpUtilsRepository::ls(Database *db, HttpHeader *h)
{
    unsigned int i;
    std::string str;
    std::string dir,fdir;

    std::vector<FileData>::iterator is;
    std::map<std::string, std::string> status;


    int onlydir          = ( h->vars["noleaf"] != "" && h->vars["noleaf"] != "0" );
    FileDataSort qs_type = (FileDataSort)atoi(h->vars["sorttyp"].c_str());

    dir = getDir(h->vars["dirInput.old"]);

    if ( ! h->error_found )
    {
        fdir = dir.substr(( rootpath + repository).size() + 1);
        readdir( dir, h->vars["pointdirInput.old"] == "1");
    }

    if ( ! h->error_found )
        read_status(dir, status);

    if ( ! h->error_found )
    {
        quicksort( dirs,  qs_type, 0,  dirs.size() - 1);
        quicksort( files, qs_type, 0, files.size() - 1);
    }

    if ( h->error_found )
    {
        DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
        return;
    }

    h->content_type = "text/json";

    DbHttpProvider::add_content(h, "{  \"ids\" : [\"menuid\", \"item\", \"action\", \"typ\", \"pos\", \"status\" ],\n"
                                   "  \"typs\" : [ 2, 2, 2, 2, 2, 2 ],\n"
                                   "  \"labels\" : [\"menuid\", \"item\", \"action\", \"typ\", \"pos\", \"status\" ],\n"
                                   "  \"formats\" : [ \"\",\"\",\"\",\"\",\"\",\"\" ],\n"
                                   "  \"values\" : [\n");

    if ( dir != "" && dir.substr(dir.length() - 1) != std::string(DIRSEP) )
        dir = dir + DIRSEP;

    const char* format = "[\"%s\", \"%s\",\"%s\",\"%s\",\"%d\", \"%s\" ]";
    std::string komma; komma = "";

    i = 0;
    for ( is= dirs.begin(); is != dirs.end(); ++is )
    {
        char str[1024];
        str[sizeof(str) - 1] =  '\0';
        std::string fullname = fdir + (*is).name;
        const char *action = "submenu";
        const char *leaf = "";
        std::string st =  ((status.find((*is).name) != status.end()) ? status.find((*is).name)->second : "");

        if ( onlydir && hassubdirs(rootpath + fullname, 0) == 0 )
            leaf = "leaf";

        snprintf(str, sizeof(str) - 1, "\"root\" : \"%s\", \"fullname\" : \"%s\", \"name\" : \"%s\", \"createtime\" : %ld, \"modifytime\" : %ld, \"accesstime\" : %ld, \"filetype\": \"dir\"", root.c_str(), fullname.c_str(), (*is).name.c_str(), (*is).statbuf.st_ctime, (*is).statbuf.st_mtime, (*is).statbuf.st_atime );
        DbHttpProvider::add_content(h, (komma + format).c_str(), fullname.c_str(), (*is).name.c_str(), ToString::mkjson("{ \"action\" : \"" + std::string(action) + "\"," "  \"parameter\" : [ \"" +  ToString::mkjson(fullname.c_str()) + "\",\"" + (*is).name.c_str() + "\", { " + str + " } ] }").c_str(), leaf, i++, st.c_str());

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
        std::string st =  ((status.find((*is).name) != status.end()) ? status.find((*is).name)->second : "");

        snprintf(str, sizeof(str) - 1, "\"root\" : \"%s\", \"fullname\" : \"%s\", \"name\" : \"%s\", \"createtime\" : %ld, \"modifytime\" : %ld, \"accesstime\" : %ld, \"filetype\": \"%s\"", root.c_str(), fullname.c_str(), (*is).name.c_str(), (*is).statbuf.st_ctime, (*is).statbuf.st_mtime, (*is).statbuf.st_atime, ft );
        DbHttpProvider::add_content(h, (komma + format).c_str(), fullname.c_str(), (*is).name.c_str(), ToString::mkjson("{ \"action\" : \"show\"," "  \"parameter\" : [ \"" +  ToString::mkjson(fullname.c_str()) + "\",\"" + (*is).name.c_str() + "\", { " + str + " } ] }").c_str(), "leaf", i++, st.c_str());

        komma = ',';
    }

    DbHttpProvider::add_content(h, "]");
    return;

}

void DbHttpUtilsRepository::dblog_update(Database *db, HttpHeader *h)
{
    unsigned int i;
    CsList cmd;
    CsList log;
    std::string filename;
    std::string lasthash;

    DbTable *tab;
    DbTable::ValueMap where, vals;
    DbConnect::ResultMat *r;

    execlog = "";
    if (( h->vars["no_vals"] == "" || h->vars["no_vals"] == "false" )  )
    {
        filename = h->vars["filenameInput.old"];

        if ( h->error_found )
            return;

        cmd.add(gitcmd.c_str());

        cmd.add("log");
        cmd.add("--pretty=%H");

        if ( exec(&cmd, getRepositoryRoot().c_str()) != 0 )
        {
            msg.perror(E_MKREPOSITORY,"Fehler während des Listens der Änderungsnotizen");
            msg.line("%s", execlog.c_str());
            return;
        }
        log.setString(execlog, '\n');
        lasthash = log[0];

        struct stat statbuf;
        if ( lstat((getRepositoryRoot() + filename).c_str(), &statbuf) == 0 && S_ISREG(statbuf.st_mode) )
        {
            cmd.setString(gitcmd.c_str());

            cmd.add("log");
            cmd.add("--name-only");
            cmd.add("--follow");
            cmd.add("--pretty=%H@%an@%at@%s");
            cmd.add(filename.c_str());

            if ( exec(&cmd, getRepositoryRoot().c_str()) != 0 )
            {
                msg.perror(E_MKREPOSITORY,"Fehler während des Listens der Änderungsnotizen");
                msg.line("%s", execlog.c_str());
                return;
            }
        }
        else
        {
            return;
        }
    }

    tab = db->p_getTable("mne_repository", "filedata");
    where["repositoryid"] = vals["repositoryid"] = h->vars["repositoryidInput.old"];
    where["filename"] = vals["filename"] = filename;
    where["hash"] = vals["hash"] = "";

    vals["repauthor"] = "";
    vals["repdate"] = (long)time(NULL);
    vals["repnote"] = "";
    vals["shortrev"] = h->vars["shortrevInput"];

    tab->del(&where);
    if ( h->vars["status"] != "" )
        tab->insert(&vals);

    where.clear();
    where["repositoryid"] = vals["repositoryid"] = h->vars["repositoryidInput.old"];
    where["filename"] = vals["filename"] = filename;

    log.setString (execlog, '\n');
    for ( i=0; i<log.size(); i = i+3 )
    {
        if ( log[i] != "" )
        {
            CsList ele(log[i],'@');
            CsList svals("hash,repnote,shortrev");

            vals["hash"] = ele[0];
            vals["repauthor"] = ele[1];
            vals["repdate"] = ele[2];
            vals["repnote"] = ele[3];
            vals["shortrev"] = "";
            vals["origname"] = log[i+2];

            where["repdate"] = ele[2];

            r = tab->select(&svals, &where);
            if (r->empty() )
                tab->insert(&vals);
            else
            {
                std::string hash = (char*)(((*r)[0])[0]);
                std::string repnote = (char*)(((*r)[0])[1]);
                std::string shortrev = (char*)(((*r)[0])[2]);

                if ( i == 0 && ele[0] == lasthash && ele[3] != repnote )
                {
                    CsList ll;

                    cmd.setString(gitcmd.c_str());
                    cmd.add("commit");
                    cmd.add("--amend");
                    cmd.add("-m");
                    cmd.add(repnote.c_str());

                    if ( exec(&cmd, getRepositoryRoot().c_str()) != 0 )
                    {
                        msg.perror(E_MKREPOSITORY,"Fehler während des Korrigierens der Änderungsnotiz");
                        msg.line("%s", execlog.c_str());
                        msg.line("%s", cmd.getString().c_str());
                        return;
                    }

                    cmd.setString(gitcmd.c_str());

                    cmd.add("log");
                    cmd.add("--pretty=%H");

                    if ( exec(&cmd, getRepositoryRoot().c_str()) != 0 )
                    {
                        msg.perror(E_MKREPOSITORY,"Fehler während des Listens der Änderungsnotizen");
                        msg.line("%s", execlog.c_str());
                        return;
                    }
                    ll.setString(execlog, '\n');
                    hash = lasthash = ll[0];

                }

                if ( ele[0] != hash )
                {
                    vals["hash"] = hash;
                    vals["repnote"] = repnote;
                    vals["shortrev"] = shortrev;
                    vals["origname"] = log[i+2];
                    tab->modify(&vals, &where);
                }

            }
        }
    }

    tab->end();
    db->release(tab);
}

void DbHttpUtilsRepository::dblog(Database *db, HttpHeader *h)
{
    (*h->vars.p_getVars())["filenameInput.old"] = (getDir(h->vars["dirInput.old"]) + h->vars["filenameInput.old"]).substr(getRepositoryRoot().size());

    if ( h->error_found == 0 )
        dblog_update(db, h);
    else
        DbHttpProvider::add_content(h, "{ \"result\" : \"error\"" );

    if ( h->error_found == 0 )
        query.data_json(db, h);

}

void DbHttpUtilsRepository::download(Database *db, HttpHeader *h)
{
    char buffer[10240];
    int rlen;
    int file;
    std::string name;
    std::string origname;
    std::string path;

    Process p;

    name = h->vars["filenameInput.old"];
    if ( name.rfind("/") != std::string::npos )
        name = name.substr(name.rfind("/")+ 1);

    h->content_type = "application/octet-stream";

    if ( (path = getDir(h->vars["dirnameInput.old"])) == "" )
    {
        h->status = 404;
        return;
    }

    if ( h->vars["hash"] == "" )
    {
#if defined(__MINGW32__) || defined(__CYGWIN__)
        if ( ( file = open ((path + DIRSEP + h->vars["filenameInput.old"]).c_str(), O_RDONLY )) < 0 )
#else
        if ( ( file = open ((path + DIRSEP + h->vars["filenameInput.old"]).c_str(), O_RDONLY | O_CLOEXEC )) < 0 )
#endif
        {
            DbHttpProvider::add_content(h,  msg.get("Datei <%s> wurde nicht gefunden").c_str(), h->vars["filenameInput.old"].c_str());
            h->status = 404;
            h->content_type = "text/plain";
            return;
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", h->vars.url_decode( h->vars["filenameInput.old"]).c_str());
            buffer[sizeof(buffer) -1] = '\0';
            h->extra_header.push_back(buffer);
            while ( ( rlen = ::read(file, buffer, sizeof(buffer))) > 0 )
                DbHttpProvider::add_contentb(h, buffer, rlen );
            close(file);
        }
    }
    else
    {
        CsList cmd;
        DbConnect::ResultMat *r;
        DbTable::ValueMap where;
        DbTable *tab;
        CsList cols("origname");

        tab = db->p_getTable("mne_repository", "filedata");

        where["repositoryid"] = h->vars["repositoryidInput.old"];
        where["filename"] = h->vars["filenameInput.old"];
        where["hash"] = h->vars["hash"];
        r = tab->select(&cols, &where);
        tab->end();
        db->release(tab);

        origname = ( ! r->empty() &&  std::string((char*)((*r)[0][0])) != "" ) ? (char*)((*r)[0][0]) : h->vars["filenameInput.old"];

        TmpFile tmpfile("repdownXXXXXX", 1);

        cmd.add(gitcmd.c_str());
        cmd.add("show");
        cmd.add(h->vars["hash"] + ":" + origname);

        h->content[0] = '\0';
        h->content_length = 0;
        p.start(cmd, "pipe", getRepositoryRoot().c_str());
        while ( ( rlen = p.read(buffer, sizeof(buffer))) > 0 )
        {
            if ( rlen > 0 )
            {
                DbHttpProvider::add_contentb(h, buffer, rlen );
                fwrite( buffer, rlen, 1, tmpfile.get_fp());
            }
            else
                if ( rlen < 0 && errno != EAGAIN ) break;
        }

        tmpfile.close();

        p.wait();
        if ( p.getStatus() != 0 )
        {
            h->content_type = "text/plain";
            snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"error.txt\"");
            buffer[sizeof(buffer) -1] = '\0';
            h->extra_header.push_back(buffer);
        }
        else
        {
            cmd.clear();
            cmd.add("mimetype");
            cmd.add("--output-format");
            cmd.add("%m");
            cmd.add(tmpfile.get_name());

            h->content_type = "";
            p.start(cmd, "pipe", getRepositoryRoot().c_str());
            while ( ( rlen = p.read(buffer, sizeof(buffer))) > 0 )
            {
                if ( rlen > 0 )
                {
                    buffer[rlen - 1] = '\0';
                    h->content_type += std::string(buffer);
                }
                else
                    if ( rlen < 0 && errno != EAGAIN ) break;
            }

            snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", h->vars.url_decode( h->vars["filenameInput.old"]).c_str());
            buffer[sizeof(buffer) -1] = '\0';
            h->extra_header.push_back(buffer);
        }
    }
}

void DbHttpUtilsRepository::downall(Database *db, HttpHeader *h)
{
    char buffer[10240];
    int rlen;
    std::string personid;
    std::map<std::string,std::string>::iterator m;

    Process p;

    personid = h->vars["personidInput.old"];


    CsList cmd;
    cmd.add("mkdrawings");
    cmd.add("-user");
    cmd.add(personid);
    cmd.add("-repositoryid");
    cmd.add(h->vars["repositoryidInput.old"]);
    for (m = h->datapath.begin(); m != h->datapath.end(); ++m )
    {
        cmd.add("-root");
        cmd.add(m->first);
        cmd.add(h->dataroot + m->second);
    }

    p.start(cmd, "pipe");

    while ( ( rlen = p.read(buffer, sizeof(buffer))) > 0 )
    {
        if ( rlen > 0 )
            DbHttpProvider::add_contentb(h, buffer, rlen );
        else
            if ( rlen < 0 && errno != EAGAIN ) break;
    }

    if ( p.getStatus() == 0 )
    {
        h->content_type = "application/zip";
        snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", h->vars.url_decode(( h->vars["name"] + ".zip").c_str()).c_str());
        buffer[sizeof(buffer) -1] = '\0';
        h->extra_header.push_back(buffer);
    }
    else
    {
        h->content_type = "text/plain";
        snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", "error.txt");
        buffer[sizeof(buffer) -1] = '\0';
        h->extra_header.push_back(buffer);
    }

}

void DbHttpUtilsRepository::mkdir  ( Database *db, HttpHeader *h)
{
    std::string filename = (getDir(h->vars["dirInput.old"]) + h->vars["filenameInput"]);
    struct stat statbuf;

    if ( lstat(filename.c_str(), &statbuf) != 0 )
    {
      HttpFilesystem::mkdir(h);
      DbHttpProvider::del_content(h);
    }

    (*h->vars.p_getVars())["filenameInput.old"] = h->vars["filenameInput"] + DIRSEP + ".gitignore";
    filename = filename + DIRSEP + ".gitignore";

#if defined(__MINGW32__) || defined(__CYGWIN__)
        int file = open(filenamename.c_str(), O_WRONLY | O_CREAT, 0666 );
#else
        int file = open(filename.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0666 );
#endif
        write(file, (std::string("# ") + filename + "\n").c_str(), filename.length() + 3);
        close(file);

        addfile(db, h);
}

void DbHttpUtilsRepository::rmdir  ( Database *db, HttpHeader *h)
{
    int n = 0;
    DIR *d;
    std::string path;

    h->content_type = "text/json";
    path = getDir(h->vars["dirInput.old"]);
    (*h->vars.p_getVars())["filenameInput.old"] = ".gitignore";

    if ( ! h->error_found )
    {
        d = opendir( path.c_str() );
        if (d == NULL)
        {
            std::string e = msg.getSystemerror(errno);
            msg.perror(E_DEL, "Fehler während des Löschen eines Ordners");
            msg.line("%s", e.c_str());
        }

        while (::readdir(d) != NULL)
            if(++n > 3) break;
        closedir(d);

        if (n == 4)
            msg.perror(E_DEL, "Ordner nicht leer");
    }

    if ( ! h->error_found )
        rmfile(db, h);
    else
        DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
}

void DbHttpUtilsRepository::addfile ( Database *db, HttpHeader *h)
{
    CsList cmd;
    int result = 0;
    std::string path;

    h->content_type = "text/json";
    std::string filename = (getDir(h->vars["dirInput.old"]) + h->vars["filenameInput.old"]).substr(getRepositoryRoot().size());

    cmd.add(gitcmd.c_str());
    cmd.add("add");
    cmd.add(filename.c_str());

    if ( h->error_found == 0 && exec(&cmd, getRepositoryRoot().c_str()) != 0 )
    {
        msg.perror(E_MKREPOSITORY,"Fehler während des Hinzufügen einer Datei");
        msg.line("%s", execlog.c_str());
        result = -1;
    }

    if ( h->error_found == 0 && h->vars["autocommitInput"] != "" )
    {
        if ( h->vars["commitmessageInput"] == "" )
            (*h->vars.p_getVars())["commitmessageInput"] = msg.get("Neue Version hinzugefügt");
        data_commit(db, h);
    }

    (*h->vars.p_getVars())["dirInput.old"] = "";
    (*h->vars.p_getVars())["filenameInput.old"] = filename;

    dblog_update(db,h);

    ( result < 0 ) ? DbHttpProvider::add_content(h, "{ \"result\" : \"error\"") : DbHttpProvider::add_content(h, "{ \"result\" : \"ok\"");
    return;
}

void DbHttpUtilsRepository::mkfile ( Database *db, HttpHeader *h)
{
    h->content_type = "text/json";

    std::string str = HttpFilesystem::mkfile(h);
    if ( str != "ok" )
    {
        msg.ignore_lang = 1;
        msg.perror(E_FILENOTFOUND, str.c_str());
        msg.ignore_lang = 0;

        DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
    }
    else
    {
        (*h->vars.p_getVars())["filenameInput.old"] = h->vars["filenameInput"];
        addfile(db, h);
    }
}

void DbHttpUtilsRepository::rmfile ( Database *db, HttpHeader *h)
{
    CsList cmd;
    int result = 0;
    std::string path;
    struct stat statbuf;

    DbTable *tab;
    DbTable::ValueMap where, vals;

    h->content_type = "text/json";

    std::string filename = (getDir(h->vars["dirInput.old"]) + h->vars["filenameInput.old"]);

    if ( h->error_found == 0 && lstat(filename.c_str(), &statbuf) == 0 )
    {
        filename = filename.substr(getRepositoryRoot().size());
        if ( h->vars["statusInput.old"] == "Y" )
        {
            cmd.add("rm");
            cmd.add("-f");
            cmd.add(filename.c_str());
        }
        else
        {
            cmd.add(gitcmd.c_str());
            cmd.add("rm");
            cmd.add("--force");
            cmd.add("--quiet");
            cmd.add(filename.c_str());
        }

        if ( exec(&cmd, getRepositoryRoot().c_str()) != 0 )
        {
            msg.perror(E_MKREPOSITORY,"Fehler während des Löschens einer Datei");
            msg.line("%s", execlog.c_str());
            result = -1;
        }

        if ( h->vars["autocommitInput"] != "" )
        {
            if ( h->vars["commitmessageInput"] == "" )
                (*h->vars.p_getVars())["commitmessageInput"] = msg.get("gelöscht");

            data_commit(db, h);
            if ( execlog != "" )
            {
                msg.pwarning(W_COMMIT,"Keine Änderungen gefunden");
                msg.line("%s", execlog.c_str());
                result = -1;
            }
        }

        if ( h->error_found == 0 )
        {
            tab = db->p_getTable("mne_repository", "filedata");
            where["repositoryid"] = h->vars["repositoryidInput.old"];
            where["filename"] = filename;

            tab->del(&where,1);
            db->release(tab);

            tab = db->p_getTable("mne_repository", "fileinterests");
            where["repositoryid"] = h->vars["repositoryidInput.old"];
            where["filename"] = filename;

            tab->del(&where,1);
            db->release(tab);
        }
    }

    DbHttpProvider::add_content(h, ( result < 0 ) ?  "{ \"result\" : \"error\"" : "{ \"result\" : \"ok\"");
}

void DbHttpUtilsRepository::mv ( Database *db, HttpHeader *h)
{
    CsList cmd;
    DbTable *tab;
    DbTable::ValueMap where, vals;
    std::string path;

    std::string dirold = getDir(h->vars["dirInput.old"]);
    std::string dirnew = getDir(h->vars["dirInput"]);

    std::string oldname = h->vars["filenameInput.old"];
    std::string newname = h->vars["filenameInput"];

    h->content_type = "text/json";

    if ( ( dirold == "" && oldname == "" ) || ( dirnew == "" && newname == "" ) )
        msg.perror(E_NEEDNAME, "Der Name der Datei darf nicht leer sein");

    if ( ! h->error_found )
    {
        cmd.add(gitcmd.c_str());
        cmd.add("mv");
        cmd.add((dirold + DIRSEP + oldname).c_str());
        cmd.add((dirnew + DIRSEP + newname).c_str());

        if ( exec(&cmd, getRepositoryRoot().c_str()) != 0 )
        {
            msg.perror(E_MKREPOSITORY,"Fehler während des Umbenenens einer Datei");
            msg.line("%s", execlog.c_str());
        }
    }

    if ( ! h->error_found )
    {
        if ( h->vars["autocommitInput"] != "" )
        {
            if ( h->vars["commitmessageInput"] == "" )
                (*h->vars.p_getVars())["commitmessageInput"] = msg.get("umbenannt von: ") + h->vars["filenameInput.old"];

            data_commit(db, h);
        }
    }

    if ( ! h->error_found )
    {
        where["repositoryid"] = h->vars["repositoryidInput.old"];
        where["filename"] = dirold + DIRSEP + oldname;
        vals["filename"]  = dirnew + DIRSEP + newname;

        tab = db->p_getTable("mne_repository", "filedata");
        tab->modify(&vals, &where, 1);
        db->release(tab);

        tab = db->p_getTable("mne_repository", "fileinterests");
        tab->modify(&vals, &where, 1);
        db->release(tab);

        (*h->vars.p_getVars())["filenameInput.old"] = dirnew + DIRSEP + newname;
        dblog_update(db, h);
    }

    DbHttpProvider::add_content(h, ( h->error_found ) ?  "{ \"result\" : \"error\"" : "{ \"result\" : \"ok\"");
}

int DbHttpUtilsRepository::data_commit(Database *db, HttpHeader *h)
{
    CsList cmd;
    int result;

    DbHttpAnalyse::Client::Userprefs userprefs = DbHttpProvider::http->getUserprefs();

    cmd.add(gitcmd.c_str());
    cmd.add("commit");
    cmd.add("-a");
    cmd.add("-m");
#if defined(__MINGW32__) || defined(__CYGWIN__)
    cmd.add(ToString::from_utf8((h->vars["commitmessageInput"] == "" ) ? msg.get("Keine Notiz") : h->vars["commitmessageInput"] ));
#else
    cmd.add((h->vars["commitmessageInput"] == "" ) ? msg.get("Keine Notiz") : h->vars["commitmessageInput"] );
#endif
    cmd.add("--author=\"" + userprefs["fullname"] + " <" + userprefs["email"] + ">\"" );

    if ( ( result = exec(&cmd, getRepositoryRoot().c_str())) == 0 ) execlog = "";
    if ( result != 0  && result != 1 )
    {
        msg.perror(E_COMMIT,"Fehler während des Akzeptierens der Änderungen");
        msg.line("%s", execlog.c_str());
        return -1;
    }
    return 0;
}

void DbHttpUtilsRepository::commit ( Database *db, HttpHeader *h)
{
    h->content_type = "text/json";
    DbHttpProvider::add_content(h, ( this->data_commit(db, h) < 0 ) ?  "{ \"result\" : \"error\"" : "{ \"result\" : \"ok\"");
}


