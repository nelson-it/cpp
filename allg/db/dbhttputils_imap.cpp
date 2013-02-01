#ifdef PTHREAD
#include <pthread.h>
#endif

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
        h->content_type = "text/xml";
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

    Database *db;
    db = dbin->getDatabase();
    db->p_getConnect("", (char*)a["MailscanUser"], (char*)a["MailscanPasswd"]);

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
        fprintf(h->content,"<?xml version=\"1.0\" encoding=\"%s\"?><result><body>error</body>",  h->charset.c_str());
        db->release(tab);
        delete db;
        return;
    }

    db->release(tab);
    delete db;

    Imap imap(server, login, passwd);

    folder = imap.getFolder();

    fprintf(h->content,
                "<?xml version=\"1.0\" encoding=\"%s\"?><result>",
                h->charset.c_str());
    fprintf(h->content,"<head encoding=\"%s\">", h->charset.c_str());
    fprintf(h->content, "<d><id>folder</id><typ>2</typ><name>Folder</name><regexp><reg></reg><help></help></regexp></d>\n");
    fprintf(h->content, "<d><id>name</id><typ>2</typ><name>Name</name><regexp><reg></reg><help></help></regexp></d>\n");

    fprintf(h->content, "</head>");
    fprintf(h->content, "<body>");

    for (i = folder.begin(); i != folder.end(); ++i )
    {
        fprintf(h->content, "<r>");
        fprintf(h->content, "<v>%s</v><v>%s</v>\n", ToString::mkxml( i->first.c_str()).c_str(),ToString::mkxml( i->second.c_str()).c_str());
        fprintf(h->content, "</r>");
    }

    fprintf(h->content, "</body>");
}

void DbHttpUtilsImap::rescan_xml(Database *db, HttpHeader *h)
{
    Imap::Headers header;
    Imap::Headers::iterator i;
    Imap::Header::iterator ii;

    fprintf(h->content,
            "<?xml version=\"1.0\" encoding=\"%s\"?><result>",
            h->charset.c_str());
    fprintf(h->content,"<head encoding=\"%s\">", h->charset.c_str());
    fprintf(h->content, "<d><id>from</id><typ>2</typ><name>%s</name><regexp><reg></reg><help></help></regexp></d>\n", msg.get("Absender").c_str());
    fprintf(h->content, "<d><id>subject</id><typ>2</typ><name>%s</name><regexp><reg></reg><help></help></regexp></d>\n", msg.get("Betreff").c_str());

    fprintf(h->content, "</head>");
    fprintf(h->content, "<body>");

    if ( h->vars["no_vals"] != "true" && h->vars["no_vals"] != "t")
    {
        ImapScan imapscan(db);
        imapscan.scan(h->vars["imapmailboxid"], atoi(h->vars["fullscan"].c_str()) );
        header = imapscan.getHeader();
    }

    for ( i = header.begin(); i != header.end(); ++i )
    {
        fprintf(h->content, "<r>");
        fprintf(h->content, "<v>%s</v><v>%s</v>\n", ToString::mkxml(i->second["FROM"]).c_str(), ToString::mkxml(i->second["SUBJECT"]).c_str());
        fprintf(h->content, "</r>");
    }

    fprintf(h->content, "</body>");
}
