#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <mail/imap_scan.h>
#include <utils/tostring.h>
#include <crypt/base64.h>
#include <argument/argument.h>

#include "dbquery.h"
#include "dbhttputils_imap.h"

DbHttpUtilsImap::DbHttpUtilsImap(DbHttp *h)
          :DbHttpProvider(h),
	   msg("DbHttpUtilsImap")
{
    subprovider["folder.xml"]    = &DbHttpUtilsImap::folder_xml;
    subprovider["rescan.xml"]    = &DbHttpUtilsImap::rescan_xml;

    subprovider["folder.json"]    = &DbHttpUtilsImap::folder_json;
    subprovider["rescan.json"]    = &DbHttpUtilsImap::rescan_json;
	h->add_provider(this);
}

DbHttpUtilsImap::~DbHttpUtilsImap()
{
}

int DbHttpUtilsImap::request(Database *db, HttpHeader *h)
{

    SubProviderMap::iterator i;

    if ( ( i = subprovider.find(h->filename)) != subprovider.end() )
    {
        h->status = 200;
        h->content_type = "text/json";
       (this->*(i->second))(db, h);
        return 1;
    }

    return 0;

}



void DbHttpUtilsImap::folder_xml(Database *dbin, HttpHeader *h)
{
    Imap::Folder folder;
    Imap::Folder::iterator i;
    DbConnect::ResultMat* rm;
    CsList cols("server,login,passwd");
    DbTable::ValueMap where;

    std::string server,login,passwd;

    Argument a;
    h->content_type = "text/xml";

    Database *db;
    db = dbin->getDatabase();
    db->p_getConnect("", a["MailscanUser"], a["MailscanPasswd"]);

    DbTable *tab = db->p_getTable("mne_mail", "imapmailbox_secret");
    where["imapmailboxid"] = h->vars["imapmailboxid"];
    rm = tab->select(&cols,&where);
    tab->end();

    if ( rm->size() > 0 )
    {
        server = (std::string)(*rm)[0][0];
        login = (std::string)(*rm)[0][1];
        passwd = (std::string)(*rm)[0][2];
    }
    else
    {
        msg.perror(E_MAILBOX, "Die Mailbox <%s> wurde nicht gefunden", h->vars["imapmailboxid"].c_str());
        add_content(h, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>",  h->charset.c_str());
        db->release(tab);
        delete db;
        return;
    }

    db->release(tab);
    delete db;

    Imap imap(server, login, passwd);

    folder = imap.getFolder();

    add_content(h, 
                "<?xml version=\"1.0\" encoding=\"%s\"?><result>",
                h->charset.c_str());
    add_content(h, "<head encoding=\"%s\">", h->charset.c_str());
    add_content(h,  "<d><id>folder</id><typ>2</typ><name>Folder</name><regexp><reg></reg><help></help><mod></mod></regexp></d>\n");
    add_content(h,  "<d><id>name</id><typ>2</typ><name>Name</name><regexp><reg></reg><help></help><mod></mod></regexp></d>\n");

    add_content(h,  "</head>");
    add_content(h,  "<body>");

    for (i = folder.begin(); i != folder.end(); ++i )
    {
        add_content(h,  "<r>");
        add_content(h,  "<v>%s</v><v>%s</v>\n", ToString::mkxml( i->first.c_str()).c_str(),ToString::mkxml( i->second.c_str()).c_str());
        add_content(h,  "</r>");
    }

    add_content(h,  "</body>");
}

void DbHttpUtilsImap::folder_json(Database *dbin, HttpHeader *h)
{
    Imap::Folder folder;
    Imap::Folder::iterator i;
    DbConnect::ResultMat* rm;
    CsList cols("server,login,passwd");
    DbTable::ValueMap where;

    std::string server,login,passwd;
    std::string komma0;

    Argument a;

    Database *db;
    db = dbin->getDatabase();
    db->p_getConnect("", a["MailscanUser"], a["MailscanPasswd"]);

    DbTable *tab = db->p_getTable("mne_mail", "imapmailbox_secret");
    where["imapmailboxid"] = h->vars["imapmailboxid"];
    rm = tab->select(&cols,&where);
    tab->end();

    if ( rm->size() > 0 )
    {
        server = (std::string)(*rm)[0][0];
        login = (std::string)(*rm)[0][1];
        passwd = (std::string)(*rm)[0][2];
    }
    else
    {
        msg.perror(E_MAILBOX, "Die Mailbox <%s> wurde nicht gefunden", h->vars["imapmailboxid"].c_str());
        add_content(h, "{ \"result\" : \"error\"");
        db->release(tab);
        delete db;
        return;
    }

    db->release(tab);
    delete db;

    Imap imap(server, login, passwd);

    folder = imap.getFolder();

    add_content(h, "{\n"
            "  \"ids\"      : [ \"folder\", \"name\" ],\n"
            "  \"labels\"   : [ \"" + msg.get("Folder") + "\",\"" + msg.get("Name") + "\" ],\n"
            "  \"typs\"     : [ \"2\",\"2\" ],\n"
            "  \"formats\"  : [ \"\", \"\" ],\n"
            "  \"regexps\"  : [ \"\", \"\" ]");

    add_content(h,  ",\n  \"values\" : [\n");
    komma0 = "";

    for (i = folder.begin(); i != folder.end(); ++i )
    {
        add_content(h,  komma0 + "    [");
        add_content(h, "\"%s\",\"%s\"", ToString::mkjson(i->first).c_str(), ToString::mkjson( i->second).c_str());
        add_content(h,  " ]");
        komma0 = ",\n";

    }
    add_content(h,  "\n ]\n");
}

void DbHttpUtilsImap::rescan_xml(Database *db, HttpHeader *h)
{
    Imap::Headers header;
    Imap::Headers::iterator i;
    Imap::Header::iterator ii;

    h->content_type = "text/xml";
    add_content(h, 
            "<?xml version=\"1.0\" encoding=\"%s\"?><result>",
            h->charset.c_str());
    add_content(h, "<head encoding=\"%s\">", h->charset.c_str());
    add_content(h,  "<d><id>from</id><typ>2</typ><name>%s</name><regexp><reg></reg><help></help><mod></mod></regexp></d>\n", msg.get("Absender").c_str());
    add_content(h,  "<d><id>subject</id><typ>2</typ><name>%s</name><regexp><reg></reg><help></help><mod></mod></regexp></d>\n", msg.get("Betreff").c_str());

    add_content(h,  "</head>");
    add_content(h,  "<body>");

    if ( db->p_getConnect()->getValue(DbConnect::BOOL, h->vars["no_vals"] ) != "true" )
    {
        ImapScan imapscan(db);
        imapscan.scan(h->vars["imapmailboxid"], atoi(h->vars["fullscan"].c_str()) );
        header = imapscan.getHeader();
    }

    for ( i = header.begin(); i != header.end(); ++i )
    {
        add_content(h,  "<r>");
        add_content(h,  "<v>%s</v><v>%s</v>\n", ToString::mkxml(i->second["FROM"]).c_str(), ToString::mkxml(i->second["SUBJECT"]).c_str());
        add_content(h,  "</r>");
    }

    add_content(h,  "</body>");
}

void DbHttpUtilsImap::rescan_json(Database *db, HttpHeader *h)
{
    Imap::Headers header;
    Imap::Headers::iterator i;
    Imap::Header::iterator ii;
    std::string komma0;

    add_content(h, "{\n"
            "  \"ids\"      : [ \"from\", \"subject\" ],\n"
            "  \"labels\"   : [ \"" + msg.get("Absender") + "\",\"" + msg.get("Betreff") + "\" ],\n"
            "  \"typs\"     : [ \"2\",\"2\" ],\n"
            "  \"formats\"  : [ \"\", \"\" ],\n"
            "  \"regexps\"  : [ \"\", \"\" ]");

    add_content(h,  ",\n  \"values\" : [\n");

    if ( db->p_getConnect()->getValue(DbConnect::BOOL, h->vars["no_vals"] ) != "true" )
    {
        ImapScan imapscan(db);
        imapscan.scan(h->vars["imapmailboxid"], atoi(h->vars["fullscan"].c_str()) );
        header = imapscan.getHeader();
    }

    komma0 = "";

    for (i = header.begin(); i != header.end(); ++i )
    {
        add_content(h,  komma0 + "    [");
        add_content(h, "\"%s\",\"%s\"", ToString::mkjson(i->second["FROM"]).c_str(), ToString::mkjson(i->second["SUBJECT"]).c_str());
        add_content(h,  " ]");
        komma0 = ",\n";
    }
    add_content(h,  "\n ]\n");
}
