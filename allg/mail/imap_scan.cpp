#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <utils/tostring.h>
#include <utils/cslist.h>
#include <crypt/base64.h>
#include <mail/imap.h>

#include <db/dbquery.h>
#include <argument/argument.h>

#include "imap_scan.h"

void *ImapScanThreadMain(void *param)
{
    Message msg("ImapScanThread", 1);
    Argument a;

    ImapScanThread *p;
    p = (ImapScanThread*)param;
    time_t lastscan = 0;
    time_t interval = (long)a["ImapScanInterval"] * 60;
    time_t offset   = (long)a["ImapScanOffset"] * 60;
    long clearheader = (long)a["ImapScanClearHeader"];
    int fullscan_count = 0;

    while(1)
    {
        if ( p->abort ) break;
        if ( ( time(NULL) - lastscan ) > 86400 )
        {
            msg.pdebug(1, "Full Scan");
            p->imapscan.scan("", 1);
            msg.pdebug(1, "Full Scan Ready");
            lastscan = (( time(NULL) % 86400 ) * 86400) + offset;
            fullscan_count++;
            if ( fullscan_count == clearheader )
            {
                p->imapscan.clearHeader();
                fullscan_count = 0;
            }
        }
        else
        {
            msg.pdebug(1, "Differenz Scan");
            p->imapscan.scan("", 0);
            msg.pdebug(1, "Differenz Scan Ready");
        }
#if defined(__MINGW32__) || defined(__CYGWIN__)
        Sleep(interval * 1000);
#else
        sleep(interval);
#endif
    }

    pthread_exit(NULL);
    return NULL;
}

ImapScanThread::ImapScanThread(Database *db)
               :imapscan(db)
{
    this->abort = 0;
    pthread_create(&(this->id), NULL, ImapScanThreadMain, (void *)this);
}

ImapScanThread::~ImapScanThread()
{
    this->abort = 1;
}

pthread_mutex_t ImapScan::mutex;
int ImapScan::mutex_init = 0;

ImapScan::ImapScan(Database *db)
         :msg("ImapScan")
{
     Argument a;

     this->db = db->getDatabase();
     this->db->p_getConnect("", (char*)a["MailscanUser"], (char*)a["MailscanPasswd"]);

     #ifdef PTHREAD
     if ( this->mutex_init == 0 ) pthread_mutex_init(&this->mutex,NULL);
     #endif
}

ImapScan::~ImapScan()
{
  delete db;
}

void ImapScan::scan(std::string mailboxid, int fullscan )
{
    DbTable *tab;
    DbTable *uidtab;
    DbQuery *query;
    CsList cols,scols;
    DbTable::ValueMap where;

    Imap::Headers headers;
    std::map<std::string, long> folders;
    std::vector<std::string> addresses,raddresses;
    std::vector<std::string> refnames;
    std::vector<std::string> refids;
    std::map<std::string, Mailbox> mailboxes;

    unsigned long refid,emailid;
    DbConnect::ResultMat emails;
    std::string a,e,re,r;
    DbConnect::ResultMat *rm;

    Imap::Headers::iterator i;
    Imap::Header::iterator ii;
    std::map<std::string, long>::iterator jf;
    std::map<std::string, Mailbox>::iterator mi;
   unsigned int j,k,l,m;
   int ignore_message;

#ifdef PTHREAD
   if ( pthread_mutex_trylock(&this->mutex) != 0 )
   {
       msg.pwarning(W_SCAN, "Es läuft noch ein Scan");
       return;
   }
#endif

     tab = db->p_getTable("mne_mail", "imapmailbox_secret");

    cols.setString("imapmailboxid,server,login,passwd");
    if ( mailboxid != "" )
        where["imapmailboxid"] = mailboxid;
    rm = tab->select(&cols,&where);
    tab->end();

    for ( j = 0; j < rm->size(); ++j )
    {
        mailboxes[(std::string)(*rm)[0][0]] = Mailbox((std::string)(*rm)[0][1], (std::string)(*rm)[0][2],(std::string)(*rm)[0][3]);
    }

    if ( rm->size() == 0 )
    {
        msg.perror(E_MAILBOX, "Die Mailbox <%s> wurde nicht gefunden",mailboxid.c_str());
        db->release(tab);
#ifdef PTHREAD
        pthread_mutex_unlock(&this->mutex);
#endif
        return;
    }

    cols.setString("refid,refname");
    scols.setString("sortcol");
    where.clear();
    query = db->p_getQuery();
    query->setName("mne_mail", "second_ref", &cols);
    emails = (*query->select(&where,&scols));
    refid = query->find("refid");
    emailid = query->find("refname");

    for ( k = 0; k < emails.size(); ++k )
    {
        refids.push_back((char *)emails[k][refid]);
        refnames.push_back((char *)emails[k][emailid]);
    }

    db->p_getConnect()->end();
    db->release(query);

    cols.setString("refid,email");
    scols.setString("sortcol");
    where.clear();
    query = db->p_getQuery();
    query->setName("mne_mail", "address", &cols);
    emails = (*query->select(&where,&scols));
    refid = query->find("refid");
    emailid = query->find("email");

    db->p_getConnect()->end();
    db->release(query);

    for ( mi = mailboxes.begin(); mi != mailboxes.end(); ++mi )
    {
        Imap imap(mi->second.server, mi->second.login,mi->second.passwd);

        cols.setString("folder,send");
        where.clear();
        where["checkit"] = 1;
        where["imapmailboxid"] = mi->first;
        tab = db->p_getTable("mne_mail","imapfolder");
        rm = tab->select(&cols,&where);
        for ( k = 0; k < rm->size(); ++k )
            folders.insert(std::map<std::string, long>::value_type((char *)((*rm)[k][0]),(long)((*rm)[k][1])));
        db->release(tab);

        tab = db->p_getTable("mne_crm", "file");
        uidtab = db->p_getTable("mne_mail", "imapuid");

        for ( jf = folders.begin(); jf != folders.end(); ++jf )
        {
            if ( fullscan )
                headers = imap.getHeader(jf->first, 0 );
            else
                headers = imap.getHeader(jf->first, time(NULL) - 3600 * 24 );

            for (i = headers.begin(); i != headers.end(); ++i )
            {
                std::string uid = imap.getHeaderElement(&(i->second), "MESSAGE-ID");
                msg.pdebug(1, "To %s",i->second["DATE"].c_str());
                if ( uid != "" && this->headers.find(uid) != this->headers.end() )
                        continue;

                this->headers.insert(Imap::Headers::value_type(i->first,i->second));

                addresses.clear();
                raddresses.clear();
                a = imap.getAddress(imap.getHeaderElement(&(i->second),"FROM"));
                std::transform(a.begin(), a.end(), a.begin(), (int(*)(int)) tolower);
                ( ! jf->second ) ? addresses.push_back(a) : raddresses.push_back(a);
                CsList cl(imap.getHeaderElement(&(i->second),"TO"));
                for ( k = 0; k < cl.size(); k++ )
                {
                    a = imap.getAddress(cl[k]);
                    std::transform(a.begin(), a.end(), a.begin(), (int(*)(int)) tolower);
                    ( jf->second ) ? addresses.push_back(a) : raddresses.push_back(a);
                }

                cl.setString(imap.getHeaderElement(&(i->second),"CC"));
                for ( k = 0; k < cl.size(); k++ )
                {
                    a = imap.getAddress(cl[k]);
                    std::transform(a.begin(), a.end(), a.begin(), (int(*)(int)) tolower);
                    ( jf->second ) ? addresses.push_back(a) : raddresses.push_back(a);
                }

                cl.setString(imap.getHeaderElement(&(i->second),"BCC"));
                for ( k = 0; k < cl.size(); k++ )
                {
                    a = imap.getAddress(cl[k]);
                    std::transform(a.begin(), a.end(), a.begin(), (int(*)(int)) tolower);
                    ( jf->second ) ? addresses.push_back(a) : raddresses.push_back(a);
                }

                for ( j = 0; j < addresses.size(); j++ )
                {
                    msg.pdebug(1, "check %s", addresses[j].c_str());

                    for ( k = 0; k < emails.size(); ++k )
                    {
                        e = (char *)emails[k][emailid];
                        std::transform(e.begin(), e.end(), e.begin(), (int(*)(int)) tolower);
                        if ( ToString::pattern(addresses[j].c_str(),e.c_str()) )
                        {
                            r = (char *)emails[k][refid];
                            ignore_message = 0;
                            for ( l = 0; ignore_message == 0 && l < emails.size(); ++l )
                            {
                                if ( r == (char *)emails[l][refid])
                                {
                                    re = (char *)emails[l][emailid];
                                    std::transform(re.begin(), re.end(), re.begin(), (int(*)(int)) tolower);
                                    for ( m = 0; m < raddresses.size(); ++m )
                                    {
                                        if ( re.substr(0,1) == "-" && raddresses[m] == re.substr(1) )
                                        {
                                            ignore_message = 1;
                                        }
                                    }
                                }
                            }

                            if ( ignore_message ) continue;

                            where.clear();
                            where["uid"] = uid;
                            where["imapmailboxid"] = mi->first;

                            rm = uidtab->select(&where, &where);
                            uidtab->end();

                            if ( rm->empty() )
                            {
                                where.clear();
                                where["uid"] = uid;
                                rm = tab->select(&where, &where);
                                tab->end();
                            }


                            if ( rm->empty() )
                            {
                                CryptBase64 crypt;
                                DbTable::ValueMap val;
                                CsList ts(i->second["DATE"], '(' );
                                long t = 0;

                                db->p_getConnect()->execute("SELECT CAST ( EXTRACT ( EPOCH FROM timestamp WITH TIME ZONE '" + ts[0] + "') AS INT4 )", 1);

                                if ( db->p_getConnect()->have_result() )
                                    t = (long)((*db->p_getConnect()->p_get_first_result())[0]);

                                val.clear();

                                where.clear();
                                cols.setString("secondrefid");
                                where["uid"] = imap.getHeaderElement(&(i->second), "IN-REPLY-TO");
                                rm = tab->select(&cols, &where);
                                tab->end();

                                if ( ! rm->empty() )
                                {
                                    val["secondrefid"] = (char*)((*rm)[0][0]);
                                }
                                else
                                {
                                    unsigned long j;
                                    std::string subject =  i->second["SUBJECT"];
                                    for ( j = 0; j < refnames.size(); ++j )
                                    {
                                        if ( subject.find(refnames[j]) != std::string::npos )
                                        {
                                            val["secondrefid"] = refids[j];
                                            break;
                                        }
                                    }
                                }

                                val["ocreatedate"] = t;
                                val["fileid"] = "################";
                                val["refid"] = (char *)emails[k][refid];
                                val["description"] = i->second["SUBJECT"];
                                val["data"] = crypt.encode(imap.getText(i->second["UID"]));
                                val["datatype"] = "application/octet-stream";
                                val["name"] = ToString::clear(i->second["SUBJECT"],(char*)"!@#$%^&*()_+=-{}[]|\\:;\"',./<>?~`") + ".eml";
                                val["typ"] = "email";
                                val["author"] = imap.getName(imap.getHeaderElement(&(i->second),"FROM"));
                                val["uid"] = uid;
                                tab->insert(&val);

                                val.clear();
                                val["imapuidid"] = "################";
                                val["imapmailboxid"] = mi->first;
                                val["uid"] = uid;
                                uidtab->insert(&val,1);
                            }
                            else if ( fullscan )
                            {
                                DbTable::ValueMap val;
                                val["imapmailboxid"] = mi->first;
                                val["uid"] = uid;
                                uidtab->modify(&val, &where, 1);
                            }

                        }
                    }
                }
            }
        }

        if ( fullscan )
        {
            where.clear();
            where["modifydate"] = (time(NULL) - 7 * 86400 );
            uidtab->del(&where);
            tab->end();
        }

        db->release(uidtab);
        db->release(tab);
    }
#ifdef PTHREAD
        pthread_mutex_unlock(&this->mutex);
#endif
}
