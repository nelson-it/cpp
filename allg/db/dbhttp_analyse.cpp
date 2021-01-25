#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <sys/unistd.h>
#else
#include <sys/utsname.h>
#endif
#include <unistd.h>

#include <argument/argument.h>

#include "dbhttp_analyse.h"
#include "database.h"
#include "dbquery.h"

DbHttpAnalyse::DbHttpAnalyse(ServerSocket *s, Database *db) :
	HttpAnalyse(s), TimeoutClient(s), msg("DbHttpAnalyse")
{
	Argument a;
    char str[1024];
    std::string dbuser,dbpasswd;
    Database *dbwait;

    dbuser = (std::string)a["DbCheckUser"];
    dbpasswd = (std::string)a["DbCheckPasswd"];

	this->s = s;
	this->dbtimeout = (long)a["DbHttpTimeout"];
	this->realm = std::string(a["DbHttpRealm"]);
    snprintf(str, sizeof(str), "MneHttpSessionId%d", (int)a["port"]);
    str[sizeof(str) - 1] = '\0';
    this->cookieid = str;

    this->db = db->getDatabase();

    dbwait = this->db->getDatabase();
    dbwait->p_getConnect("", dbuser, dbpasswd);
     while ( ! dbwait->have_connection() )
     {
#if defined(__MINGW32__) || defined(__CYGWIN__)
         Sleep(10000);
#else
         sleep(10);
#endif
         dbwait->p_getConnect("", dbuser, dbpasswd);
     }

    read_datadir();
    user_count = 0;
    pthread_mutex_init(&cl_mutex, NULL);
}

DbHttpAnalyse::~DbHttpAnalyse()
{
}

void DbHttpAnalyse::read_datadir()
{
    Argument a;
	Database *db;
	CsList cols;
	DbTable *tab;
	DbTable::ValueMap values;
	DbTable::ValueMap where;
	DbConnect::ResultMat *r;
    DbConnect::ResultMat::iterator ri;
    char *str;

    db = this->db->getDatabase();

    if ( (std::string)a["DbSystemUser"] == "" )
    {
        datapath.clear();
        return;
    }
    db->p_getConnect("", a["DbSystemUser"], a["DbSystemPasswd"]);
    tab = db->p_getTable(db->getApplschema(), "folder");

    cols.setString("name,location");
    r = tab->select(&cols, &where);
    tab->end();

    datapath.clear();
    for ( ri = r->begin(); ri != r->end(); ri++ )
    datapath[((char*)(*ri)[0])] = (char*)((*ri)[1]);


    db->release(tab);

    tab = db->p_getTable(db->getApplschema(), "server");
    tab->del(&where);

#if defined(Darwin) || defined(__MINGW32__) || defined(__CYGWIN__)
    str = (char *)malloc(10240);
    gethostname( str, 10240);
    values["serverid"] = str;
    where["serverid"] = str;

    getcwd(str, 10240);
    values["pwd"] = str;
    free(str);
#else
    struct utsname utsname;
    uname(&utsname);
    values["serverid"] = utsname.nodename;
    values["pwd"] = str = get_current_dir_name();
    where["serverid"] = utsname.nodename;
#endif


    r = tab->select(&values, &where);
    ( r->empty() ) ? tab->insert(&values) : tab->modify(&values, &where);
    tab->end();
    db->release(tab);

    delete db;
}

void DbHttpAnalyse::check_user(HttpHeader *h)
{
	std::string cookie;
	Clients::iterator ic;
	unsigned int client;
	client = h->client;
	cookie = h->cookies[cookieid.c_str()];

	lock();

	msg.pdebug(D_CLIENT, "cookie %s", cookie.c_str());
	if ( cookie != "" && (ic = clients.find(cookie) ) != clients.end()  )
	{
	    std::string a1,a2;

	    a1 = ic->second.host;
	    a2 = s->getHost(client);
        h->cookies.addCookie(cookieid, ic->first);

	    msg.pdebug(D_CLIENT, "prüfe auf Gleichheit %d", client);
		msg.pdebug(D_CLIENT, "host %s:%s", a1.c_str(), a2.c_str());
		msg.pdebug(D_CLIENT, "connection %d", ic->second.db->have_connection());

		if ( ic->second.host == s->getHost(client) && ( ic->second.db->have_connection() ) )
		{
		    msg.pdebug(D_CLIENT, "clients sind gleich %d", client);
			if ( ic->second.last_connect < time(NULL)) ic->second.last_connect = time(NULL);

			unlock();
			return;
		}
	}

	if ( h->dirname.find("/main/login") == 0 )
	{
	    unlock();
	    return;
	}

	char str[1024];
	Client cl;
    std::string user = h->vars["mneuserloginname"];
    std::string passwd = h->vars["mneuserpasswd"];
    h->location = h->vars["location"];

    if ( user != "" && user.substr(0,6) != "mne_")
    {
        std::string a;

        a = s->getHost(client);

        msg.pdebug(D_CLIENT, "ist ein neuer client %d", client);
        msg.pdebug(D_CLIENT, "host %s", a.c_str());
        msg.pdebug(D_CLIENT, "base %s", h->base.c_str());
        msg.pdebug(D_CLIENT, "user %s", user.c_str());
        msg.pdebug(D_CLIENT, "passwd %s", passwd.c_str() );

        sprintf(str, "%d%d", (unsigned int)time(NULL), user_count++);
        h->set_cookies[cookieid.c_str()] = str;
        h->realm = realm;

        cl.host = s->getHost(client);
        cl.base = h->base;
        cl.db = db->getDatabase();
        cl.cookie = str;
        clients[str] = cl;
        ic = clients.find(str);

        ic->second.db->p_getConnect("", user, passwd);

        if (ic->second.db->have_connection() )
        {
            msg.pdebug(D_CLIENT, "client %d hat sich verbunden", client);
            h->cookies.addCookie(cookieid, ic->first);
            ic->second.user = user;
            ic->second.passwd = passwd;
            ic->second.last_connect = time(NULL);
            setUserprefs(&ic->second);
            ic->second.db->p_getConnect()->end();
            h->status = 301;
            unlock();
            return;
        }

        msg.pdebug(D_CLIENT, "falscher client %d", client);

        delete ic->second.db;
        clients.erase(ic);

        h->set_cookies["MneHttpSessionLoginWrong"] = "1";
    }

    if ( h->vars.exists("user") )
    {
        h->set_cookies["MneHttpSessionLoginWrong"] = "1";
    }

    h->set_cookies[cookieid.c_str()] = "";

	unlock();

}

void DbHttpAnalyse::setUserprefs(Client *cl)
{
	DbConnect::ResultMat *r;
	DbTable::ValueMap where;
	std::vector<std::string> vals;
	CsList order;
	std::string str;

	str = "select mne_catalog.start_session('"+cl->db->getApplschema()+"')";
	cl->db->p_getConnect()->execute(str.c_str());
	cl->db->p_getConnect()->end();

	DbQuery *q = cl->db->p_getQuery();
    q->setName(cl->db->getApplschema(), "userpref", NULL);

	where["username"] = cl->db->p_getConnect()->getUser();

	r = q->select(&where);
	if (r->empty() )
	{
	    DbTable *t = cl->db->p_getTable(cl->db->getApplschema(), "userpref");
		DbTable::ValueMap v;
		v["username"] = where["username"];
		r = t->select(&v, &where);
		if ( r->empty() )
		{
			v["username"] = where["username"];
			t->insert(&v, 1);
		}
	    cl->db->release(t);
	    cl->db->p_getConnect()->end();
	}

	r = q->select(&where);
	if ( !r->empty() )
	{
	    std::string id;
	    unsigned int j;
	    for ( j = 0; ( id = q->getId(j) ) != ""; j++ )
	        cl->userprefs[id] = std::string(((*r)[0])[j].format());
	}

    if ( cl->userprefs.find("fullname") == cl->userprefs.end() || cl->userprefs["fullname"] == "" )
    {
	    cl->userprefs["fullname"] = cl->db->p_getConnect()->getUser();
    }

    if ( cl->userprefs.find("email") == cl->userprefs.end() || cl->userprefs["email"] == "" )
    {
	    cl->userprefs["email"] = cl->db->p_getConnect()->getUser() + "@local";
    }
    cl->db->release(q);
    cl->db->p_getConnect()->end();
}


void DbHttpAnalyse::del_client(unsigned int client)
{
    msg.pdebug(D_CLIENT, "lösche client %d", client);

    if (this->tv_sec == 0)
    {
        setWakeup(time(NULL) + this->dbtimeout);
        msg.pdebug(D_TIMEOUT, "Nächstes Aufräumen %s", Message::timestamp(this->tv_sec).c_str());
    }
}


void DbHttpAnalyse::connect(int client)
{
}

void DbHttpAnalyse::disconnect(int client)
{
    lock();
    del_client(client);
    HttpAnalyse::disconnect(client);
    unlock();
}

void DbHttpAnalyse::timeout(long sec, long usec, long w_sec, long w_usec)
{
	Clients::iterator c;
	int need_timeout = 0;

    lock();
	msg.pdebug(D_CLIENT, "Starte aufräumen");
	c = clients.begin();

	while (c != clients.end() )
	{
		for (c = clients.begin(); c != clients.end(); ++c)
		{
		    msg.pdebug(D_CLIENT, "client %s lastconnect %s", c->first.c_str(), ctime( &(c->second.last_connect)));
			if (c->second.last_connect != 0)
			{
				if (c->second.last_connect + dbtimeout <= time(NULL) )
				{
					msg.pdebug(D_CLIENT, "lösche client endgültig %s", c->first.c_str());
					c->second.lock();
					delete c->second.db;
					clients.erase(c);
					break;
				}
				else if ( c->second.last_connect + dbtimeout <= time(NULL) + this->dbtimeout )
				{
					if (need_timeout == 0 || c->second.last_connect < need_timeout )
						need_timeout = c->second.last_connect;
				}
			}
		}
	}

	msg.pdebug(D_CLIENT, "Beende aufräumen need_timeout %d", need_timeout);

	if (need_timeout == 0)
		setWakeup(0);
	else if (need_timeout + this->dbtimeout < time(NULL) + 60)
		setWakeup(time(NULL) + 60);
	else
		setWakeup(need_timeout + this->dbtimeout);

    unlock();
}

