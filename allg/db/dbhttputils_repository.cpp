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
    subprovider["modify.json"]   = &DbHttpUtilsRepository::modify_json;
    subprovider["delete.json"]   = &DbHttpUtilsRepository::delete_json;
    subprovider["data.json"]     = &DbHttpUtilsRepository::data_json;

    subprovider["ls.json"]       = &DbHttpUtilsRepository::ls_json;
    subprovider["addfile.json"]  = &DbHttpUtilsRepository::addfile_json;
    subprovider["mkfile.json"]   = &DbHttpUtilsRepository::mkfile_json;
    subprovider["rmfile.json"]   = &DbHttpUtilsRepository::rmfile_json;
    subprovider["mv.json"]       = &DbHttpUtilsRepository::mv_json;
    subprovider["mkdir.json"]    = &DbHttpUtilsRepository::mkdir_json;
    subprovider["rmdir.json"]    = &DbHttpUtilsRepository::rmdir_json;

    subprovider["commit.json"]   = &DbHttpUtilsRepository::commit_json;
    subprovider["dblog.json"]    = &DbHttpUtilsRepository::dblog_json;

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
        (*h->vars.p_getVars())["rootInput.old"] = (char*)((*r)[0][0]);
        (*h->vars.p_getVars())["nameInput.old"] = (char*)((*r)[0][1]);
    }

}

std::string DbHttpUtilsRepository::getRoot(HttpHeader *h)
{
    std::string dir = HttpFilesystem::getRoot(h);
    std::string rep = h->vars["nameInput.old"];

    if ( dir == "" )
        return dir;

    if ( rep == "")
    {
        msg.perror(E_REPOSITORY, "Der Aktenordner muss einen Namen haben");
        return "";
    }

    return dir + DIRSEP + rep;
}

std::string DbHttpUtilsRepository::getDir(HttpHeader *h, int errormsg )
{
    std::string retval = HttpFilesystem::getDir(h, 0);

    if ( errormsg && retval == "" )
        msg.perror(E_FILENOTFOUND, "Der Ordner <%s> wurde nicht gefunden", (h->vars["rootInput.old"] + ":" + h->vars["nameInput.old"]).c_str());
    return retval;
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

    (*h->vars.p_getVars())["rootInput.old"] = h->vars["rootInput"];
    if ( (root = HttpFilesystem::getRoot(h)) == "" )
    {
        msg.perror(E_MKREPOSITORY,"Der Aktenschrank <%s> existiert nicht", h->vars["rootInput"].c_str());
        return -1;
    }

    if ( check_path(root.c_str(), h->vars["nameInput"].c_str(), 1, 0) != "" )
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
        if ( check_path(root.c_str(), templ.c_str(), 1, 0) != "" && h->vars["nameInput"] != templ && templ != "" )
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

int DbHttpUtilsRepository::modify (Database *db, HttpHeader *h)
{
    std::string root;
    std::string name;

    CsList cmd;
    int result;

    root = h->vars["rootInput.old"];
    name = h->vars["nameInput.old"];

    if ( HttpFilesystem::getRoot(h) == "" || check_path(HttpFilesystem::getRoot(h).c_str(), name.c_str()) == "" )
    {
        msg.perror(E_MKREPOSITORY,"Der Aktenordner <%s> existiert nicht", (root + ":" + name).c_str());
        return -1;
    }

    if ( name == "" || h->vars["nameInput"] == "" )
    {
        msg.perror(E_MKREPOSITORY,"Die Aktenordner müssen eine Namen haben");
        return -1;
    }

    if ( name != h->vars["nameInput"] )
    {
        cmd.add("mv");
        cmd.add(name);
        cmd.add(h->vars["nameInput"]);

        Process p;
        p.start(cmd, NULL, HttpFilesystem::getRoot(h).c_str());
        result = p.wait();

        if ( result != 0 )
        {
            std::string str = msg.getSystemerror(errno);
            msg.perror(E_MKREPOSITORY,"Fehler während des Umbenennen eines Aktenordners");
            msg.line("%s", str.c_str());
            return -1;
        }
    }

    (*h->vars.p_getVars()).erase("rootInput.old");
    (*h->vars.p_getVars()).erase("nameInput.old");

    DbHttpProvider::del_content(h);
    return 0;
}


void DbHttpUtilsRepository::modify_json (Database *db, HttpHeader *h)
{
    h->status = 200;
    h->content_type = "text/json";
    if ( this->modify(db, h) != 0 )
        DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
    else
        table.modify_json(db, h);
}


int DbHttpUtilsRepository::del (Database *db, HttpHeader *h)
{
    std::string root;

     time_t rawtime;
     struct tm * timeinfo;
     char buffer [80];

     time (&rawtime);
     timeinfo = gmtime (&rawtime);

     strftime (buffer,80,"%Y%m%d%H%M%S",timeinfo);

    if ( (root = HttpFilesystem::getRoot(h)) == "" )
    {
        msg.perror(E_MKREPOSITORY,"Der Aktenordner <%s> existiert nicht", (h->vars["rootInput.old"] + ":" + h->vars["nameInput.old"]).c_str());
        return -1;
    }

    if ( h->vars["nameInput.old"] == "" )
     {
         msg.perror(E_DELREPOSITORY,"Der Aktenordner muss eine Namen haben");
         return -1;
     }

    if ( check_path(root.c_str(), h->vars["nameInput.old"].c_str(), 1, 0) == "" )
    {
        msg.pwarning(E_DELREPOSITORY,"Der Aktenordner <%s> existiert nicht", h->vars["nameInput.old"].c_str());
    }
#if defined(__MINGW32__) || defined(__CYGWIN__)
    else
    {
        SetFileAttributes(( root + DIRSEP + h->vars["nameInput.old"] ).c_str(), FILE_ATTRIBUTE_NORMAL);
        if ( ! MoveFile(( root + DIRSEP + h->vars["nameInput.old"] ).c_str(), ( root + DIRSEP + ".trash" + DIRSEP + h->vars["nameInput.old"] + "_" + buffer).c_str()) )
        {
            std::string str = msg.getSystemerror(GetLastError());
            msg.perror(E_DELREPOSITORY,"Fehler während des Löschens eines Aktenordners");
            msg.line("%s", str.c_str());
            return -1;
        }
    }
#else
    else if ( rename( ( root + DIRSEP + h->vars["nameInput.old"] ).c_str(), ( root + DIRSEP + ".trash" + DIRSEP + h->vars["nameInput.old"] + "_" + buffer).c_str()) != 0 )
    {
        std::string str = msg.getSystemerror(errno);
        msg.perror(E_DELREPOSITORY,"Fehler während des Löschens eines Aktenordners");
        msg.line("%s", str.c_str());
        return -1;
    }
#endif
    (*h->vars.p_getVars()).erase("rootInput.old");
    (*h->vars.p_getVars()).erase("nameInput.old");

    return 0;
}

void DbHttpUtilsRepository::delete_json (Database *db, HttpHeader *h)
{
    h->status = 200;
    h->content_type = "text/json";
    if ( this->del(db, h) != 0 )
        DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
    else
    {
        (*h->vars.p_getVars())["table"] = "filedata";
        table.delete_json(db,h);

        (*h->vars.p_getVars())["table"] = "fileinterests";
        table.delete_json(db,h);

        DbHttpProvider::del_content(h);
        (*h->vars.p_getVars())["table"] = "repository";
        table.delete_json(db,h);
    }

}

void DbHttpUtilsRepository::dbdata_checkdir(HttpHeader *h, std::string dir)
{
    HttpVars vars = h->vars;
    std::vector<FileData>::iterator is;
    std::vector<FileData> dirs;
    std::string root;
    int f;
    root = getRoot(h);

    if ( dir != "" )
        (*h->vars.p_getVars())["dirInput.old"] = (*h->vars.p_getVars())["dirInput.old"] + DIRSEP + dir;

    std::string file = ( h->vars["dirInput.old"] + DIRSEP + ".gitignore");
    if ( check_path(root.c_str(), file.c_str(), 1, 0) == "" )
    {
#if defined(__MINGW32__) || defined(__CYGWIN__)
        f = open((root + DIRSEP + file).c_str(), O_WRONLY | O_CREAT, 0666 );
#else
        f = open((root + DIRSEP + file).c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0666 );
#endif
        write(f, (std::string("# ") + file + "\n").c_str(), file.length() + 3);
        close(f);
    }

    HttpFilesystem::readdir(h);
    dirs = this->dirs;
    for ( is = dirs.begin(); is != dirs.end(); ++is )
        dbdata_checkdir(h, (*is).name);

    h->vars = vars;
}

void DbHttpUtilsRepository::dbdata_update ( Database *db, HttpHeader *h)
{
    DbTable *tab;
    DbTable::ValueMap where,vals;
    DbConnect::ResultMat *r;
    std::string root;

    std::string schema;
    std::string table;

    if ( h->vars["wcol"] != "" )
    {
        CsList wcol(h->vars["wcol"]);
        CsList wval(h->vars["wval"]);
        unsigned int i;
        for ( i = 0; i< wcol.size(); i++)
        {
            if ( wcol[i] == "root")
            {
                (*h->vars.p_getVars())["rootInput.old"] = wval[i];
                break;
            }
        }
    }

    if ( h->vars["rootInput.old"] == "" )
        return;

    schema = h->vars["schema"];
    table = h->vars["table"];
    tab = db->p_getTable(schema, table);


    vals["repositoryid"] = "################";
    where["root"] = vals["root"] = h->vars["rootInput.old"];
    where["name"] = vals["name"] = h->vars["nameInput.old"];

    r = tab->select(&vals, &where);
    if (r->empty() )
        tab->insert(&vals);
    db->release(tab);

    root = getRoot(h);
    if ( check_path(root.c_str(), (std::string(DIRSEP) + ".git").c_str(), 1, 0) == "" )
    {
        CsList cmd;
        int result;

        dbdata_checkdir(h, "");

        cmd.add(gitcmd.c_str());
        cmd.add("init");
        cmd.add("--quiet");
        cmd.add("--shared=group");
        cmd.add(h->vars["nameInput.old"]);

        Process p;
        p.start(cmd, NULL, HttpFilesystem::getRoot(h).c_str());
        result = p.wait();

        if ( result != 0 )
            msg.perror(E_MKREPOSITORY,"Fehler während des Erzeugens der Versionsverwaltung");

        cmd.clear();
        cmd.add(gitcmd.c_str());
        cmd.add("add");
        cmd.add(".");

        p.start(cmd, NULL, root.c_str());
        result = p.wait();

        if ( result != 0 )
        {
            msg.perror(E_MKREPOSITORY,"Fehler während des Hinzufügens der Ignoredateien der Versionsverwaltung");
            return;
        }
        (*h->vars.p_getVars())["autocommitInput"] = "1";
        (*h->vars.p_getVars())["commitmessageInput"] = "Initialversion";
        commit(db, h);
    }
}

void DbHttpUtilsRepository::data_json(Database *db, HttpHeader *h)
{
    dbdata_update(db,h);
    query.data_json(db, h);
}

int DbHttpUtilsRepository::ls(Database *db, HttpHeader *h, std::map<std::string, std::string> &status)
{
    unsigned int i;
    std::string str;
    CsList cmd;
    int result;
    int first;
    std::string dir;
    std::vector<FileData>::iterator is;


    readdir(h);
    if ( h->error_found )
    {
        return -1;
    }

    cmd.add(gitcmd.c_str());
    cmd.add("status");
    cmd.add("-s");

    result = exec(&cmd, getRoot(h).c_str());

    if ( result != 0 )
    {
        msg.perror(E_MKREPOSITORY,"Fehler während des Listens eines Ordners");
        msg.line("%s", execlog.c_str());
        return -1;
    }

    CsList s(execlog, '\n', 1);
    ;
    for ( i = 0; i<s.size(); ++i)
    {
        std::string name = s[i].substr(3);
        std::string st = s[i].substr(0,2);
        std::string::size_type n;

        if ( (n = name.find(" -> ")) != std::string::npos ) name = name.substr(n + 4);
        if ( name[0] == '"') name = name.substr(1,name.find('"', 1) - 1);

        if ( st == "??" ) st = "Y";
        st.erase( std::remove_if( st.begin(), st.end(), ::isspace ), st.end() );

        status.insert(std::make_pair(name, st));

        dir = name;
        first = 1;
        while ( ( n = dir.rfind('/') ) != std::string::npos )
        {
            name = dir.substr(n + 1);
            dir = dir.substr(0,n);

            if ( status.find(dir) == status.end() )
                status.insert(std::make_pair(dir, "M"));


            for ( is = files.begin(); is != files.end(); ++is) if ( (*is).name == name ) break;
            if ( first && dir == this->dir && is == files.end() && st == "D" )
            {
                FileData d;
                d.name = name;
                files.push_back(d);
            }
            first = 0;
        }
        for ( is = files.begin(); is != files.end(); ++is) if ( (*is).name == name ) break;
        if ( first == 1 && this->dir == "" && is == files.end() && st == "D" )
        {
            FileData d;
            d.name = name;
            files.push_back(d);
        }
    }
    return 0;
}

void DbHttpUtilsRepository::ls_json(Database *db, HttpHeader *h)
{
    int i;
    std::vector<FileData>::iterator is;
    std::map<std::string, std::string> status;
    std::string dir;

    h->content_type = "text/json";

    if ( this->ls(db, h, status) < 0 )
    {
       DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
       return;
    }

    int onlydir = ( h->vars["noleaf"] != "" );
    int singledir = ( h->vars["singledir"] != "" );

    std::string rootname = h->vars["rootname"];
    std::string idname = h->vars["idname"];

    idname = ( idname == "" ) ? "name" : idname;
    rootname = ( rootname == "" ) ? "root" : rootname;

    DbHttpProvider::add_content(h, "{  \"ids\" : [\"menuid\", \"item\", \"action\", \"typ\", \"pos\", \"status\" ],\n"
                                   "  \"typs\" : [ 2, 2, 2, 2, 2, 2 ],\n"
                                   "  \"labels\" : [\"menuid\", \"item\", \"action\", \"typ\", \"pos\", \"status\" ],\n"
                                   "  \"formats\" : [ \"\",\"\",\"\",\"\",\"\",\"\" ],\n"
                                   "  \"values\" : [\n");

    dir = this->dir;
    if ( dir != "" && dir.substr(dir.length() - 2) != DIRSEP )
        dir = dir + DIRSEP;

    i = 0;
    const char* format = "[\"%s\", \"%s\",\"%s\",\"%s\",\"%d\",\"%s\" ]";
    std::string komma = "";
    for ( is= dirs.begin(); is != dirs.end(); ++is )
    {
        std::string fullname = dir + (*is).name;
        std::string st =  ((status.find(fullname) != status.end()) ? status.find(fullname)->second : "");
        fullname = ToString::substitute(fullname, DIRSEP, "/");

        if ( singledir )
            DbHttpProvider::add_content(h, (komma + format).c_str(), fullname.c_str(), (*is).name.c_str(), ToString::mkjson("{ \"action\" : \"show\", \"parameter\" : [ \"" +  ToString::mkjson(fullname.c_str()) + "\",\"" + (*is).name.c_str() + "\", { \"repositoryid\" : \"" +  h->vars["repositoryidInput.old"] + "\", \"" + rootname + "\" : \"" + root + "\", \"" + idname + "\" : \"" +  ToString::mkjson(fullname.c_str()) + "\", \"status\" : \"" + st + "\"} ] }").c_str(), "leaf", i++, st.c_str() );
        else
            DbHttpProvider::add_content(h, (komma + format).c_str(), fullname.c_str(), (*is).name.c_str(), ToString::mkjson("{ \"action\" : \"submenu\", \"parameter\" : \"\" }").c_str(), "", i++, st.c_str() );

        komma = ',';
    }
    for ( is= files.begin(); !onlydir && is != files.end(); ++is )
    {
        std::string fullname = dir + (*is).name;
        std::string st =  ((status.find(fullname) != status.end()) ? status.find(fullname)->second : "");
        fullname = ToString::substitute(fullname, DIRSEP, "/");
        DbHttpProvider::add_content(h, (komma + format).c_str(), fullname.c_str(), (*is).name.c_str(), ToString::mkjson("{ \"action\" : \"show\", \"parameter\" : [ \"" +  ToString::mkjson(fullname.c_str()) + "\",\"" + (*is).name.c_str() + "\", { \"repositoryid\" : \"" +  h->vars["repositoryidInput.old"] + "\", \"" + rootname + "\" : \"" + root + "\", \"" + idname + "\" : \"" +  ToString::mkjson(fullname.c_str()) + "\", \"status\" : \"" + st + "\"} ] }").c_str(), "leaf", i++, st.c_str() );

        komma = ',';
    }

    DbHttpProvider::add_content(h, "]");

}

void DbHttpUtilsRepository::dblog_update(Database *db, HttpHeader *h)
{
    unsigned int i;
    std::string str;
    CsList cmd;
    int result;
    DbTable *tab;
    DbTable::ValueMap where, vals;
    DbConnect::ResultMat *r;
    std::string lasthash;
    CsList l;

    execlog = "";
    if (( h->vars["no_vals"] == "" || h->vars["no_vals"] == "false" )  )
    {

        if ( getRoot(h) == "" )
            return;

        cmd.add(gitcmd.c_str());

        cmd.add("log");
        cmd.add("--pretty=%H");

        result = exec(&cmd, getRoot(h).c_str());

        if ( result != 0 )
        {
            msg.perror(E_MKREPOSITORY,"Fehler während des Listens der Änderungsnotizen");
            msg.line("%s", execlog.c_str());
            return;
        }
        l.setString(execlog, '\n');
        lasthash = l[0];

        if ( check_path(h, h->vars["filenameInput.old"], 0, 0 ) != "" && S_ISREG(statbuf.st_mode))
        {
            cmd.setString(gitcmd.c_str());

            cmd.add("log");
            cmd.add("--name-only");
            cmd.add("--follow");
            cmd.add("--pretty=%H@%an@%at@%s");
            cmd.add(ToString::substitute(h->vars["filenameInput.old"].c_str(), "/", DIRSEP));

            result = exec(&cmd, getRoot(h).c_str());

            if ( result != 0 )
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
    where["filename"] = vals["filename"] = h->vars["filenameInput.old"];
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
    where["filename"] = vals["filename"] = h->vars["filenameInput.old"];

    l.setString (execlog, '\n');
    for ( i=0; i<l.size(); i = i+3 )
    {
        if ( l[i] != "" )
        {
            CsList ele(l[i],'@');
            CsList svals("hash,repnote,shortrev");

            vals["hash"] = ele[0];
            vals["repauthor"] = ele[1];
            vals["repdate"] = ele[2];
            vals["repnote"] = ele[3];
            vals["shortrev"] = "";
            vals["origname"] = l[i+2];

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

                    exec(&cmd, getRoot(h).c_str());
                    if ( result != 0 )
                    {
                        msg.perror(E_MKREPOSITORY,"Fehler während des Korrigierens der Änderungsnotiz");
                        msg.line("%s", execlog.c_str());
                        msg.line("%s", cmd.getString().c_str());
                        return;
                    }

                    cmd.setString(gitcmd.c_str());

                    cmd.add("log");
                    cmd.add("--pretty=%H");

                    result = exec(&cmd, getRoot(h).c_str());

                    if ( result != 0 )
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
                    vals["origname"] = l[i+2];
                    tab->modify(&vals, &where);
                }

            }
        }
    }

    tab->end();
    db->release(tab);
}

void DbHttpUtilsRepository::dblog_json(Database *db, HttpHeader *h)
{
    if ( h->vars["repositoryidInput.old"] != "" )
        dblog_update(db, h);

    if ( h->error_found == 0 )
    {
        DbHttpProvider::del_content(h);
        query.data_json(db, h);
    }
    else
        DbHttpProvider::add_content(h, "{ \"result\" : \"error\"" );
}

void DbHttpUtilsRepository::download(Database *db, HttpHeader *h)
{
    char buffer[10240];
    int rlen;
    int file;
    std::string name;
    std::string origname;

    Process p;

    name = h->vars["filenameInput.old"];
    if ( name.rfind("/") != std::string::npos )
        name = name.substr(name.rfind("/")+ 1);

    h->content_type = "application/octet-stream";

    if ( getDir(h) == "" )
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
        p.start(cmd, "pipe", getRoot(h).c_str());
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
            p.start(cmd, "pipe", getRoot(h).c_str());
            while ( ( rlen = p.read(buffer, sizeof(buffer))) > 0 )
            {
                if ( rlen > 0 )
                {
                    buffer[rlen] = '\0';
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

int DbHttpUtilsRepository::mkdir  ( Database *db, HttpHeader *h)
{
    HttpFilesystem::mkdir(h);
    DbHttpProvider::del_content(h);

    (*h->vars.p_getVars())["filenameInput"] = h->vars["filenameInput"] + DIRSEP + ".gitignore";
    std::string name = h->vars["filenameInput"];

#if defined(__MINGW32__) || defined(__CYGWIN__)
        int file = open((path + DIRSEP + name).c_str(), O_WRONLY | O_CREAT, 0666 );
#else
        int file = open((path + DIRSEP + name).c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0666 );
#endif
        write(file, (std::string("# ") + name + "\n").c_str(), name.length() + 3);
        close(file);
        return 0;
}

void DbHttpUtilsRepository::mkdir_json ( Database *db, HttpHeader *h)
{
    this->mkdir(db, h);
    this->addfile_json(db, h);
}


int DbHttpUtilsRepository::rmdir  ( Database *db, HttpHeader *h)
{
    int n = 0;
    DIR *d;

    if ( getDir(h) == "" )
    {
        return -1;
    }

    d = opendir(( path + + DIRSEP + h->vars["filenameInput.old"]).c_str() );
    if (d == NULL)
    {
        std::string e = msg.getSystemerror(errno);
        msg.perror(E_DEL, "Fehler während des Löschen eines Ordners");
        msg.line("%s", e.c_str());
        return -1;
    }

    while (::readdir(d) != NULL)
      if(++n > 3) break;
    closedir(d);

    if (n == 4)
    {
        msg.perror(E_DEL, "Ordner nicht leer");
        return -1;
    }

    std::string hdir = h->vars["dirInput.old"];
    std::string hname = h->vars["filenameInput.old"];

    (*h->vars.p_getVars())["dirInput.old"] = h->vars["dirInput.old"] + DIRSEP + h->vars["filenameInput.old"];
    (*h->vars.p_getVars())["filenameInput.old"] = ".gitignore";
    DbHttpProvider::del_content(h);

    if ( rmfile(db, h) < 0 )
        return -1;

    std::string name = check_path(h, "", 0, 0);
    if (  name != "" )
    {
        DbHttpProvider::del_content(h);
        h->error_found = 0;
        h->error_messages.clear();

       HttpFilesystem::rmdir(h);
    }

    return 0;
}

void DbHttpUtilsRepository::rmdir_json ( Database *db, HttpHeader *h)
{
    h->content_type = "text/json";
    DbHttpProvider::add_content(h, ( this->rmdir(db, h) < 0 ) ?  "{ \"result\" : \"error\"" : "{ \"result\" : \"ok\"");
}


int DbHttpUtilsRepository::addfile ( Database *db, HttpHeader *h)
{
    CsList cmd;
    int result;

    if ( getDir(h) == "" )
        return -1;

    std::string filename = path + DIRSEP + h->vars["filenameInput"];

    cmd.add(gitcmd.c_str());
    cmd.add("add");
    cmd.add(filename.c_str());

    result = exec(&cmd, getRoot(h).c_str());

    if ( result != 0 )
    {
        msg.perror(E_MKREPOSITORY,"Fehler während des Hinzufügen einer Datei");
        msg.line("%s", execlog.c_str());
        return -1;
    }

    if ( h->error_found == 0 && h->vars["autocommitInput"] != "" )
    {
        if ( h->vars["commitmessageInput"] == "" )
            (*h->vars.p_getVars())["commitmessageInput"] = msg.get("Neue Version hinzugefügt");
        commit(db, h);
    }

    (*h->vars.p_getVars())["dirInput.old"] = "";
    (*h->vars.p_getVars())["filenameInput.old"] = ToString::substitute(filename.substr(getRoot(h).size() + 1), DIRSEP, "/");

    dblog_update(db,h);

    return 0;
}

void DbHttpUtilsRepository::addfile_json ( Database *db, HttpHeader *h)
{
    h->content_type = "text/json";

    if ( this->addfile(db, h) < 0 )
        DbHttpProvider::add_content(h, "{ \"result\" : \"error\"");
    else
        DbHttpProvider::add_content(h, "{ \"result\" : \"ok\"");
}

void DbHttpUtilsRepository::mkfile_json ( Database *db, HttpHeader *h)
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
        addfile_json(db, h);
    }
}

int DbHttpUtilsRepository::rmfile ( Database *db, HttpHeader *h)
{
    CsList cmd;
    int result;

    DbTable *tab;
    DbTable::ValueMap where, vals;

    if ( getDir(h) == "" )
        return -1;

    if ( check_path(path, h->vars["filenameInput.old"], true, false ) == "" )
        return 0;

    if ( h->vars["statusInput.old"] == "Y" )
    {
        cmd.add("rm");
        cmd.add("-f");
        cmd.add((path + DIRSEP + h->vars["filenameInput.old"]).c_str());
    }
    else
    {
        cmd.add(gitcmd.c_str());
        cmd.add("rm");
        cmd.add("--force");
        cmd.add("--quiet");
        cmd.add((path + DIRSEP + h->vars["filenameInput.old"]).c_str());
    }

    result = exec(&cmd, getRoot(h).c_str());

    if ( result != 0 )
    {
        msg.perror(E_MKREPOSITORY,"Fehler während des Löschens einer Datei");
        msg.line("%s", execlog.c_str());
        return -1;
    }

    if ( h->vars["autocommitInput"] != "" )
    {
        if ( h->vars["commitmessageInput"] == "" )
            (*h->vars.p_getVars())["commitmessageInput"] = msg.get("gelöscht");

        commit(db, h);
        if ( execlog != "" )
        {
            msg.pwarning(W_COMMIT,"Keine Änderungen gefunden");
            msg.line("%s", execlog.c_str());
            return -1;
        }
    }

    tab = db->p_getTable("mne_repository", "filedata");
    where["repositoryid"] = h->vars["repositoryidInput.old"];
    where["filename"] = ToString::substitute((path + "/" + h->vars["filenameInput.old"]).substr(getRoot(h).size() + 1), DIRSEP, "/");

    tab->del(&where,1);
    db->release(tab);

    tab = db->p_getTable("mne_repository", "fileinterests");
    where["repositoryid"] = h->vars["repositoryidInput.old"];
    where["filename"] = ToString::substitute((path + "/" + h->vars["filenameInput.old"]).substr(getRoot(h).size() + 1), DIRSEP, "/");

    tab->del(&where,1);
    db->release(tab);

    return 0;
}

void DbHttpUtilsRepository::rmfile_json ( Database *db, HttpHeader *h)
{
    h->content_type = "text/json";
    DbHttpProvider::add_content(h, ( this->rmfile(db, h) < 0 ) ?  "{ \"result\" : \"error\"" : "{ \"result\" : \"ok\"");
}

int DbHttpUtilsRepository::mv ( Database *db, HttpHeader *h)
{
    CsList cmd;
    int result;
    DbTable *tab;
    DbTable::ValueMap where, vals;

    std::string oldname = h->vars["filenameInput.old"];
    std::string newname = h->vars["filenameInput"];

    if ( oldname == "" || newname == "" )
        msg.perror(E_NEEDNAME, "Der Name der Datei darf nicht leer sein");

    if ( getDir(h) == "" || oldname == "" || newname == "" )
        return -1;

    cmd.add(gitcmd.c_str());
    cmd.add("mv");
    cmd.add((path + DIRSEP + oldname));
    cmd.add((path + DIRSEP + newname).c_str());

    result = exec(&cmd, getRoot(h).c_str());

    if ( result != 0 )
    {
        msg.perror(E_MKREPOSITORY,"Fehler während des Umbenenens einer Datei");
        msg.line("%s", execlog.c_str());
        return -1;
    }

    if ( h->vars["autocommitInput"] != "" )
    {
        if ( h->vars["commitmessageInput"] == "" )
            (*h->vars.p_getVars())["commitmessageInput"] = msg.get("umbenannt von: ") + h->vars["filenameInput.old"];

        commit(db, h);
    }

    where["repositoryid"] = h->vars["repositoryidInput.old"];
    where["filename"] = ToString::substitute((path + "/" + h->vars["filenameInput.old"]).substr(getRoot(h).size() + 1), DIRSEP, "/");
    vals["filename"]  = ToString::substitute((path + "/" + h->vars["filenameInput"]).substr(getRoot(h).size() + 1), DIRSEP, "/");

    tab = db->p_getTable("mne_repository", "filedata");
    tab->modify(&vals, &where, 1);
    db->release(tab);

    tab = db->p_getTable("mne_repository", "fileinterests");
    tab->modify(&vals, &where, 1);
    db->release(tab);

    (*h->vars.p_getVars())["filenameInput.old"] = ToString::substitute((path + "/" + h->vars["filenameInput"]).substr(getRoot(h).size() + 1), DIRSEP, "/");
    dblog_update(db, h);

    return 0;
}

void DbHttpUtilsRepository::mv_json ( Database *db, HttpHeader *h)
{
    h->content_type = "text/json";
    DbHttpProvider::add_content(h, ( this->mv(db, h) < 0 ) ?  "{ \"result\" : \"error\"" : "{ \"result\" : \"ok\"");
}

int DbHttpUtilsRepository::commit(Database *db, HttpHeader *h)
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

    if ( ( result = exec(&cmd, getRoot(h).c_str())) == 0 ) execlog = "";
    if ( result != 0  && result != 1 )
    {
        msg.perror(E_COMMIT,"Fehler während des Akzeptierens der Änderungen");
        msg.line("%s", execlog.c_str());
        return -1;
    }
    return 0;
}

void DbHttpUtilsRepository::commit_json ( Database *db, HttpHeader *h)
{
    h->content_type = "text/json";
    DbHttpProvider::add_content(h, ( this->commit(db, h) < 0 ) ?  "{ \"result\" : \"error\"" : "{ \"result\" : \"ok\"");
}


