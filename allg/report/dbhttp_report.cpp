#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <sys/stat.h>

#include <db/dbtable.h>
#include <db/dbquery.h>

#include <utils/process.h>
#include <utils/tostring.h>
#include <crypt/base64.h>

#define MKREPORT "mkreport"
#define MKHEADER "mkheader"
#define MKPDF    "mkpdf"

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <unistd.h>
#define DIRSEP   "\\"
#define unlink DeleteFile
#else
#define DIRSEP   "/"
#endif

#include "dbhttp_report.h"
#include "report_tex.h"

DbHttpReport::DbHttpReport(DbHttp *h)
:DbHttpProvider(h),
 msg("DbHttpReport")
{
    subprovider["header.html"] = &DbHttpReport::header_html;
    h->add_provider(this);
}

DbHttpReport::~DbHttpReport()
{
}

DbTable::ValueMap DbHttpReport::mk_pdfwhere(HttpHeader *h, CsList *pdfwcol, CsList *pdfwval)
{
    DbTable::ValueMap where;
    unsigned int i;
    std::string::size_type j;

    CsList wcol(h->vars["wcol"]);
    CsList wval(h->vars["wval"]);

    for ( i=0; i<pdfwcol->size(); ++i)
    {
        if ( i < pdfwval->size())
        {
            if ( (*pdfwval)[i][0] == '#')
                where[(*pdfwcol)[i]] = (*pdfwval)[i].substr(1);
            else if ( ( j = wcol.find(((*pdfwval)[i]))) != std::string::npos && j < wval.size())
                where[(*pdfwcol)[i]] = wval[j];
            else
            {
                msg.perror(E_PDF_WVAL, "wval <%s> nicht gefunden", ((*pdfwval)[i]).c_str() );
                return where;
            }
        }
        else
        {
            msg.perror(E_PDF_WHERE, "Falsche Anzahl für Pdfwhere");
            return where;
        }
    }
    return where;
}

void DbHttpReport::start_function(Database *db, DbQuery *query, DbConnect::ResultVec *rv, std::string schema, std::string function, CsList cols )
{
    std::string stm,komma;
    unsigned int j;

    if ( schema == "" || function == "" )
        return;

    stm = "SELECT " + schema + "." + function + "( ";
    for ( j = 0; j < cols.size(); ++j)
    {
        if ( query->getColtyp(j) == DbConnect::CHAR )
            stm += komma
            + "E'"
            +  ToString::mascarade((char*)((*rv)[query->find(cols[j])]), "'\\")
        + "'";
        else
            stm += komma + ((*rv)[j]).format();

        komma = ", ";
    }

    stm += ")";

    db->p_getConnect()->execute(stm.c_str());
}

int DbHttpReport::request(Database *db, HttpHeader *h)
{

    std::string str;
    std::string t;
    std::string::size_type j;


    SubProviderMap::iterator i;

    if ( (i = subprovider.find(h->filename)) != subprovider.end() )
    {
        (this->*(i->second))(db, h);
        return 1;
    }

    if ( ( j = h->filename.find_last_of(".") ) != std::string::npos )
    {
        str = h->filename.substr(0, j );
        t = h->filename.substr(j+1);
    }
    else
    {
        str = h->filename;
        t = "html";
    }


    h->content_type = "text/plain";

    if ( str == "autoreport" )
        mk_auto(db, h);
    else
        index(db, h, str.c_str());

    if ( h->error_messages.size() == 0 )
    {
        if ( t == "html" || t == "pdf" )
            h->content_type = "application/pdf";
        else
            h->content_type = "mneprint/pdf";
    }

    return 1;
}

void DbHttpReport::header_html( Database *db, HttpHeader *h)
{
    std::string u;
    DbHttpAnalyse::Client::Userprefs::iterator ui;
    DbHttpAnalyse::Client::Userprefs userprefs = this->http->getUserprefs();
    int result;
    CsList cmd;

    h->content_type = "text/plain";

    if ( ! h->vars.exists("companyownprefix") )
    {
        fprintf(h->content, "kein companyownprefix angegeben");
        return;
    }

    cmd.add(MKHEADER);
    userprefs["uowncompanyownprefix"] = h->vars["companyownprefix"];
    for ( ui = userprefs.begin(); ui != userprefs.end(); ++ui)
    {
        if ( ui->second != "" )
        {
            cmd.add("-" + ui->first);
            cmd.add(ui->second);
        }
    }

    cmd.add("-typ");
    cmd.add((h->vars["typ"] == "" ) ? "report" : h->vars["typ"]);

    cmd.add("-filetyp");
    cmd.add(h->vars["data"].substr(10));

#if defined(__MINGW32__) || defined(__CYGWIN__)
    cmd.add(ToString::substitute(ToString::substitute(h->vars.getFile("data").c_str(), "\\", "/"), "C:", "/cygdrive/c"));
    std::string s = "bash -c 'export PATH=`pwd`:$PATH; ";
    unsigned int j;
    for ( j = 0; j<cmd.size(); j++)
        s += " \"" + ToString::mascarade(cmd[j].c_str(), "\"") + "\"";
    s += "'";
    cmd.clear();
    cmd.add(s);
#else
    cmd.add(h->vars.getFile("data").c_str());
#endif

    fclose(h->content);

    Process p(this->http->getServersocket());
    p.start( cmd, h->content_filename.c_str(), NULL, NULL, NULL, 1);
    result = p.wait();

    if ( ( h->content = fopen(h->content_filename.c_str(), "rb+")) != NULL )
    {
        fseek(h->content, 0, SEEK_END);
        if ( result == 0 )
            fprintf(h->content,"ok");
        else
            fprintf(h->content, "notok");
    }
}

void DbHttpReport::mk_auto( Database *dbin, HttpHeader *h)
    {

    Database *db;
    db = dbin->getDatabase();
    db->p_getConnect("", h->user, h->passwd);
    if ( db->p_getConnect() == NULL ) return;

    db->p_getConnect()->execute(("select mne_catalog.start_session('" + db->getApplschema()+"')").c_str());

    DbQuery *query;
    DbTable *tab = db->p_getTable("mne_application", "reportsauto");
    CsList cols("report,repwcol,repwval,repwop,selschema,selquery,selwcol,selwval,selwop,selsort,startschema,startfunction,readyschema,readyfunction,status");
    DbTable::ValueMap where;
    DbTable::ValueMap values;
    DbConnect::ResultMat r;
    unsigned int i,j;

    unsigned int pdfcount;
    char str[10240];
    char* dir;

    std::string content_filename;

    h->status = 200;

    ((DbHttp*)this->http)->unlock_client();

    pdfcount = 0;

    std::string report,repwcol,repwval,repwop,selschema,selquery,startschema,startfunction,readyschema,readyfunction;
    CsList selwcol,selwval,selwop,selsort;
    long status;

    where["schema"] = h->vars["schema"];
    where["reportsautoid"] = h->vars["autoreport"];

    r = *(tab->select(&cols,&where));
    if ( r.size() > 0 )
    {
        report = (char*)((r)[0][0]);
        repwcol= (char*)((r)[0][1]);
        repwval= (char*)((r)[0][2]);
        repwop = (char*)((r)[0][3]);
        selschema = (char*)((r)[0][4]);
        selquery = (char*)((r)[0][5]);
        selwcol.setString((char*)((r)[0][6]));
        selwval.setString((char*)((r)[0][7]));
        selwop.setString((char*)((r)[0][8]));
        selsort.setString((char*)((r)[0][9]));
        startschema = (char*)((r)[0][10]);
        startfunction = (char*)((r)[0][11]);
        readyschema = (char*)((r)[0][12]);
        readyfunction = (char*)((r)[0][13]);
        status = (long)((r)[0][14]);
    }


    if ( report == "" )
    {
        msg.pwarning(W_NOREPORT, "Der Autoreport <%s> ist unbekannt", ( h->vars["schema"] + "." + h->vars["autoreport"]).c_str());
        db->release(tab);
        db->p_getConnect()->end();
        delete db;
        return;
    }
    if (  status != 0 )
    {
        msg.pwarning(W_REPORTRUNNING, "Der Autoreport <%s> ist schon gestartet", ( h->vars["schema"] + "." + h->vars["autoreport"]).c_str());
        db->release(tab);
        db->p_getConnect()->end();
        delete db;
        return;
    }

    *str = '\0';
    if ( getenv ("TMPDIR") != NULL)
    {
        strncpy(str, getenv("TMPDIR"), sizeof(str) -1 );
        strncat(str, DIRSEP, sizeof(str) - strlen(str) - 1);
    }
    else if ( getenv ("TMP") != NULL)
    {
        strncpy(str, getenv("TMP"), sizeof(str) -1 );
        strncat(str, DIRSEP, sizeof(str) - strlen(str) - 1);
    }
    strncat(str, "HttpReportAutoXXXXXX", sizeof(str) - strlen(str) - 1);
    mkdtemp(str);

/*
#if defined(__MINGW32__) || defined(__CYGWIN__)
    mkdir(str);
#else
    mkdir(str,0700);
#endif

*/
    dir = &str[strlen(str)];

    values["status"] = time(NULL);
    tab->modify(&values, &where);

    cols.setString(repwval);
    where.clear();

    if ( h->vars.exists("selwcol") ) selwcol = h->vars["selwcol"];
    if ( h->vars.exists("selwop")  ) selwop  = h->vars["selwop"];
    if ( h->vars.exists("selwval") ) selwval = h->vars["selwval"];

    query = db->p_getQuery();
    query->setName(selschema,selquery, NULL);
    r = *(query->select(&selwcol,&selwval,&selwop,&selsort));

    db->p_getConnect()->end();

    db->p_getConnect()->start();
    content_filename = h->content_filename;

    if (  r.size() == 0 && h->vars["rowwarning"] != "false" )
        msg.pwarning(W_NOROWS, "Der Autoreport <%s> hat keine Zeilen", ( h->vars["schema"] + "." + h->vars["autoreport"]).c_str());
    else if ( r.size() == 0 && h->vars["rowwarningtext"] != "" )
        h->error_messages.push_back(h->vars["rowwarningtext"].c_str());


    (*h->vars.p_getVars())["sqlend"] = "0";
    (*h->vars.p_getVars())["wcol"] = repwcol;
    (*h->vars.p_getVars())["wop"] = repwop;
    (*h->vars.p_getVars())["schema"] = "";

    for ( i = 0; i<r.size() && h->error_messages.size() == 0; ++i)
    {
        CsList list;
        CsList cols(repwcol);

        sprintf(dir, "%sreport%d", DIRSEP, pdfcount++);

        fclose(h->content);
        h->content_filename = str;
        if ( ( h->content = fopen(h->content_filename.c_str(), "wb+")) == NULL )
            msg.perror(E_AUTO_TMPFILE,"Kann Tempfile <%s> nicht öffnen", h->content_filename.c_str());

        if ( h->error_messages.size() == 0 )
            this->start_function(db, query, &r[i], startschema, startfunction, cols);

        for ( j=0; j<cols.size(); ++j)
            list.add((r[i][query->ofind((cols[j]))]).format());

        (*h->vars.p_getVars())["wval"] = list.getString();
        if ( query->find("language") != std::string::npos && (std::string)r[i][query->find("language")] != "" )
            (*h->vars.p_getVars())["language"] = (char *)r[i][query->find("language")];
        else
            (*h->vars.p_getVars())["language"] = "de";

        if ( h->error_messages.size() == 0 )
            index(db,h,report.c_str());

        if ( h->error_messages.size() == 0 )
            this->start_function(db, query, &r[i], readyschema, readyfunction, cols);

    }

    db->p_getConnect()->end();

    values["status"] = 0;
    tab->modify(&values, &where);

    db->release(tab);
    db->release(query);
    db->p_getConnect()->end();
    delete db;

    if ( h->content != NULL ) fclose(h->content);
    h->content_filename = content_filename;

    if ( h->error_messages.size() == 0 && r.size() > 0  )
    {
        *dir = '\0';
        int status;
        CsList cmd;

#if defined(__MINGW32__) || defined(__CYGWIN__)
        cmd.add(std::string("bash -c 'export PATH=`pwd`:$PATH; ") + MKPDF);
        cmd.add(ToString::substitute(ToString::substitute(h->content_filename.c_str(), "\\", "/"), "C:", "/cygdrive/c"));
        cmd.add(ToString::substitute(ToString::substitute(str, "\\", "/"), "C:", "/cygdrive/c"));
#else
        cmd.add(MKPDF);
        cmd.add(h->content_filename);
        cmd.add(str);
#endif

        Process p(this->http->getServersocket());
        p.start(cmd, NULL, NULL, NULL, NULL, 1);
        if ( ( status = p.wait()) != 0 )
            msg.perror(E_AUTO_STATUS, "Status des Kindprozesses ist <%d>", status);
    }

    for ( i=0; i<pdfcount; i++)
    {
        sprintf(dir, "%sreport%d", DIRSEP, i);
        unlink(str);
    }

    *dir = '\0';
    rmdir(str);

    h->content = fopen(h->content_filename.c_str(), "rb+");
    fseek(h->content, 0, SEEK_END);


}

void DbHttpReport::index( Database *db, HttpHeader *h, const char *str)
{
    CsList    sort;
    CsList    wid, wval, wop;
    unsigned i;
    std::string::size_type n;

    ReportTex report;
    ReportTex::Macros macros;
    ReportTex::Macros xml;

    DbTable *tab = NULL;
    DbTable::ValueMap where;
    CsList cols;
    DbConnect::ResultMat *r;

    std::string pdfschema;
    std::string pdftable;
    CsList pdfwcol,pdfwval,pdfwop;

    DbHttpAnalyse::Client::Userprefs::iterator ui;

    h->status = 200;

    tab = db->p_getTable("mne_application","reportscache");
    where["reportscacheid"] = str;
    cols.setString("repschema,reptable,repwcol,repwop,repwval");
    r = tab->select(&cols, &where);
    if ( r->size() > 0 )
    {
        pdfschema = ((std::string)((*r)[0][0]));
        pdftable = ((std::string)((*r)[0][1]));
        pdfwcol.setString(((std::string)((*r)[0][2])));
        pdfwop.setString(((std::string)((*r)[0][3])));
        pdfwval.setString(((std::string)((*r)[0][4])));
    }
    db->release(tab);

    if ( pdfschema != "" && pdftable != "" )
    {
        CsList cols("pdf");
        DbTable::ValueMap where = mk_pdfwhere(h, &pdfwcol, &pdfwval);

        where["pdf"] = "";
        pdfwop.add("<>");

        tab = db->p_getTable(pdfschema,pdftable);
        r = tab->select(&cols, &where, &pdfwop);
        if ( r->size() > 0 )
        {
            CryptBase64 base64;
            int len,clen;
            clen = ((*r)[0][0]).length;
            unsigned char *str = (unsigned char *)new char[clen];
            len = base64.decode((unsigned char*)((char *)(*r)[0][0]), str, strlen((char *)(*r)[0][0]));
            fwrite(str, len, 1, h->content);
            db->release(tab);
            delete str;

            if ( h->vars["sqlend"] == "1" )
                db->p_getConnect()->end();

            return;
        }
    }

    sort.setString(h->vars["sort"]);
    wop.setString(h->vars["wop"]);
    wid.setString(h->vars["wcol"]);
    wval.setString(h->vars["wval"]);

    for (i=0; 1; ++i )
    {
        char str[256];
        sprintf(str, "macro%d", i );

        if ( (n = h->vars[str].find(',')) != std::string::npos )
            macros[h->vars[str].substr(0,n)] = ToString::mktex(h->vars[str].substr(n+1), 1);
        else
            break;
    }

    for (i=0; 1; ++i )
    {
        char str[256];
        sprintf(str, "xml%d", i );

        if ( (n = h->vars[str].find(',')) != std::string::npos )
            xml[h->vars[str].substr(0,n)] = h->vars[str].substr(n+1);
        else
            break;
    }

    HttpVars::Vars *vars;
    HttpVars::Vars::iterator iv;
    vars =h->vars.p_getVars();
    for ( iv=vars->begin(); iv != vars->end(); iv++)
    {
    	if ( iv->first.size() > 9 && iv->first.substr(iv->first.size() - 9 ) == "Input.old" )
    	{
            std::string colid = iv->first.substr(0, iv->first.size() - 9 );
            wid.add(colid);
            if ( h->vars.exists(colid + "Op.old") )
                wop.add(h->vars[colid + "Op.old"]);
            else
                wop.add("=");
            wval.add(iv->second);

    	}
    }

    report.userprefs = this->http->getUserprefs();
    if ( report.mk_report(db,str,0,h->content, h->vars["language"], h->vars["schema"], h->vars["query"], &wid, &wval, &wop, &sort, &macros, &xml ) < 0 )
    {
        msg.pwarning(W_NOROWS, "Der Report <%s> hat keine Zeilen", str);
    }

    if ( h->error_messages.empty() )
    {
    	char str[512];
    	*str = '\0';
    	str[sizeof(str) -1] = '\0';
        char filename[512];

#if defined(__MINGW32__) || defined(__CYGWIN__)
    	*filename = '\0';
    	if ( getenv ("TEMP") != NULL)
    	{
    		strncpy(filename, getenv("TEMP"), sizeof(filename) -1 );
    		strncat(filename, "\\HttpReportXXXXXX", sizeof(filename) - strlen(str) - 1);
    	}
    	_mktemp_s(filename, strlen(filename) + 1);
    	filename[sizeof(filename) - 1] = '\0';
#else
    	if ( getenv ("TMPDIR") != NULL)
    	{
    		strncpy(filename, getenv("TMPDIR"), sizeof(str) -1 );
    		strncat(filename, "/", sizeof(str) - strlen(str) - 1);
    	}
    	else if ( getenv ("TMP") != NULL)
    	{
    		strncpy(filename, getenv("TMP"), sizeof(str) -1 );
    		strncat(filename, "/", sizeof(str) - strlen(str) - 1);
    	}
    	else
    	{
    	    strcpy(filename, "/tmp/");
    	}
    	strncat(filename, "HttpReportXXXXXX", sizeof(str) - strlen(str) - 1);

    	int fd = mkstemp(filename);
    	if ( fd != -1 )
    	{
    	    close(fd);
    	    unlink(filename);
    	}
#endif
    	HttpTranslate trans;
        trans.make_answer(h, NULL);

        fclose(h->content);

        CsList cmd;
        cmd.add(MKREPORT);

        if ( report.getLandscape() )
            cmd.add("-landscape");

        for ( ui = report.userprefs.begin(); ui != report.userprefs.end(); ++ui)
        {
        	if ( ui->second != "" )
        	{
        	    cmd.add("-" + ui->first);
        	    cmd.add(ui->second);
        	}
        }

#if defined(__MINGW32__) || defined(__CYGWIN__)
        cmd.add(ToString::substitute(ToString::substitute(h->content_filename.c_str(), "\\", "/"), "C:", "/cygdrive/c"));
        std::string s = "bash -c 'export PATH=`pwd`:$PATH; ";
        unsigned int j;
        for ( j = 0; j<cmd.size(); j++)
            s += " \"" + ToString::mascarade(cmd[j].c_str(), "\"") + "\"";
        s += "'";
        cmd.clear();
        cmd.add(s);
#else
        cmd.add(h->content_filename);
#endif
        Process p(this->http->getServersocket());
        p.start(cmd, filename, NULL, NULL, NULL, 1);
        p.wait();

        if ( ( h->content = fopen(h->content_filename.c_str(), "rb+")) != NULL )
        {
            char str[16];
            if ( fread(str, 4, 1, h->content) != 1 )
        	{
        		msg.perror(E_PDF_READ, "konnte Pdfdaten nicht lesen");
        	}
        	str[4] = '\0';
        	if ( strncmp((char*)str, "%PDF", 4) != 0 )
        	{
        		msg.perror(E_PDF_HEADER, "Datei ist keine PDF Datei");
                FILE *fp;
                char buffer[1024];

                fclose(h->content);
                h->content = fopen(h->content_filename.c_str(), "wb+");
                h->content_type = "text/plain";

                if ( ( fp = fopen(filename, "r")) != NULL )
                {
                    while ( fgets(buffer, sizeof(buffer), fp) != NULL )
                    {
                        while ( buffer[strlen(buffer) - 1] == '\n' ||
                                buffer[strlen(buffer) - 1] == '\r' )
                            buffer[strlen(buffer) - 1] = '\0';

                        if ( *buffer != '\0' )
                            msg.line("%s", buffer);
                    }
                }
                if ( fp != NULL ) fclose(fp);
        	}
        	else
        	{
        	    fseek(h->content, 0, SEEK_END);
        	    if ( ( pdfschema != "" && pdftable != "" ) || h->vars["base64"] != "" )
        	    {
        	        DbTable::ValueMap where;
        	        DbTable::ValueMap values;
        	        CsList cols("pdf");
        	        int len,clen;

        	        len = ftell(h->content);
        	        clen = (((len + 3 ) / 3 * 4 + 76) / 76) * 77 + 77;
        	        unsigned char *str = (unsigned char*)new char[len + 1];
        	        unsigned char *crypt = (unsigned char*)new char[ clen + 2];
        	        rewind(h->content);

        	        if ( fread(str, len, 1, h->content) != 1 )
        	        {
        	            msg.perror(E_PDF_READ, "konnte Pdfdaten nicht lesen");
        	            fseek(h->content, 0, SEEK_END);
        	        }
        	        else if ( strncmp((char*)str, "%PDF", 4) == 0 )
        	        {
        	            CryptBase64 base64;
        	            int l;
        	            crypt[( l = base64.encode(str, crypt, len))] = '\0';

        	            if ( pdfschema != "" && pdftable != "" )
        	            {
        	                values["pdf"] = (char *)crypt;
        	                DbTable::ValueMap where = mk_pdfwhere(h, &pdfwcol, &pdfwval);
        	                tab->modify(&values, &where);
        	                db->release(tab);
        	                fseek(h->content, 0, SEEK_END);
        	            }
        	            else if ( h->vars["base64schema"] != "" && h->vars["base64table"] != "" )
        	            {
        	                DbTable *tab = db->p_getTable(h->vars["base64schema"],h->vars["base64table"]);
        	                DbTable::ValueMap vals;
        	                DbTable::ValueMap where;

        	                where[h->vars["base64id"]] = h->vars["base64idvalue"];
        	                vals[h->vars["base64data"]]= std::string((char *)crypt);

        	                h->content_type = "text/plain";
        	                rewind(h->content);

        	                if ( tab->modify(&vals, &where) == 0 )
        	                {
        	                    fprintf(h->content, "ok");
        	                }
        	                else
        	                {
        	                    fprintf(h->content, "error");
        	                }
        	                db->release(tab);
        	            }
        	            else
        	            {
        	                rewind(h->content);
        	                fprintf(h->content,"%s", crypt);
        	            }
        	        }
        	        delete str;
        	        delete crypt;
        	    }
            }
        }
        else
        {
            FILE *fp;
            char buffer[1024];

            h->content = fopen(h->content_filename.c_str(), "wb+");

            h->content_type = "text/plain";
            msg.perror(E_PDF_OPEN, "Konnte Pdf-Datei nicht öffnen");

            if ( ( fp = fopen(filename, "r")) != NULL )
            {
                while ( fgets(buffer, sizeof(buffer), fp) != NULL )
                {
                    while ( buffer[strlen(buffer) - 1] == '\n' ||
                            buffer[strlen(buffer) - 1] == '\r' )
                        buffer[strlen(buffer) - 1] = '\0';

                    if ( *buffer != '\0' )
                        msg.line("%s", buffer);
                }
            }
            if ( fp != NULL ) fclose(fp);
        }
        unlink(filename);
    }
    else
    {
        h->content_type = "text/plain";
        rewind(h->content);
    }

    if ( h->vars["sqlend"] == "1" )
        db->p_getConnect()->end();

    return;

}

