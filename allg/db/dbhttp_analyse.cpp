#ifdef PTHREAD
#include <pthread.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include <argument/argument.h>

#include "dbhttp_analyse.h"
#include "database.h"
#include "dbquery.h"

DbHttpAnalyse::DbHttpAnalyse(ServerSocket *s, Database *db) :
	HttpAnalyse(s), TimeoutClient(s), msg("DbHttpAnalyse")
{
	Argument a;
    char str[1024];

	this->s = s;
	this->dbtimeout = a["DbHttpTimeout"];
	this->realm = (char*)a["DbHttpRealm"];
    snprintf(str, sizeof(str), "MneHttpSessionId%d", (int)a["port"]);
    str[sizeof(str) - 1] = '\0';
    this->cookieid = str;


    this->db = db->getDatabase();

    read_datadir();
    user_count = 0;

#ifdef PTHREAD
	pthread_mutex_init(&cl_mutex, NULL);
#endif
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
	DbTable::ValueMap where;
	DbConnect::ResultMat *r;
    DbConnect::ResultMat::iterator ri;

    db = this->db->getDatabase();

    db->p_getConnect("", (char*)a["DbSystemUser"], (char*)a["DbSystemPasswd"]);
    tab = db->p_getTable(db->getApplschema(), "folder");

    cols.setString("name,location");
    r = tab->select(&cols, &where);
    tab->end();

    datapath.clear();
    for ( ri = r->begin(); ri != r->end(); ri++ )
    datapath[((char*)(*ri)[0])] = (char*)((*ri)[1]);

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

#ifdef PTHREAD
	pthread_mutex_lock(&cl_mutex);
#endif

	msg.pdebug(D_CLIENT, "cookie %s", cookie.c_str());
	if ( cookie != "" && (ic = clients.find(cookie) ) != clients.end()  )
	{
		msg.pdebug(D_CLIENT, "prüfe auf Gleichheit %d", client);
		msg.pdebug(D_CLIENT, "host %d:%d", ic->second.host, s->getHost(client));
		msg.pdebug(D_CLIENT, "browser %d:%d", ic->second.browser, h->browser);
		msg.pdebug(D_CLIENT, "user-agent %s:%s", ic->second.user_agent.c_str(), h->user_agent.c_str());
		msg.pdebug(D_CLIENT, "connection %d", ic->second.db->have_connection());

		if (   ic->second.host == s->getHost(client)
			&& ic->second.browser == h->browser
            && ( ic->second.user_agent == h->user_agent || h->browser == HttpHeader::B_IE )
			&& ( ic->second.db->have_connection() ) )
		{
		    msg.pdebug(D_CLIENT, "clients sind gleich %d", client);
			ic->second.last_connect = time(NULL);

#ifdef PTHREAD
			pthread_mutex_unlock(&cl_mutex);
#endif
			return;
		}
	}

	if ( h->dirname == "/main/login" )
	{
#ifdef PTHREAD
	    pthread_mutex_unlock(&cl_mutex);
#endif
	    return;
	}

	char str[1024];
	Client cl;
    std::string user = h->vars["user"];
    std::string passwd = h->vars["passwd"];
    h->location = h->vars["location"];

    if ( user != "" && user.substr(0,6) != "mneerp")
    {
        msg.pdebug(D_CLIENT, "ist ein neuer client %d", client);
        msg.pdebug(D_CLIENT, "host %d", s->getHost(client));
        msg.pdebug(D_CLIENT, "browser %d", h->browser);
        msg.pdebug(D_CLIENT, "user-agent %s", h->user_agent.c_str());
        msg.pdebug(D_CLIENT, "user %s", user.c_str());
        msg.pdebug(D_CLIENT, "passwd %s", passwd.c_str() );

        sprintf(str, "%d%d", (unsigned int)time(NULL), user_count++);
        h->set_cookies[cookieid.c_str()] = str;
        h->realm = realm;

        cl.host = s->getHost(client);
        cl.browser = h->browser;
        cl.user_agent = h->user_agent;
        cl.db = db->getDatabase();
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
#ifdef PTHREAD
    pthread_mutex_unlock(&cl_mutex);
#endif
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

#ifdef PTHREAD
	pthread_mutex_unlock(&cl_mutex);
#endif

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
        cl->userprefs["fullname"] = cl->db->p_getConnect()->getUser();

    if ( cl->userprefs.find("email") == cl->userprefs.end() || cl->userprefs["email"] == "" )
        cl->userprefs["email"] = cl->db->p_getConnect()->getUser() + "@local";

	cl->db->release(q);
	cl->db->p_getConnect()->end();
}


void DbHttpAnalyse::del_client(unsigned int client)
{
    msg.pdebug(D_CLIENT, "lösche client %d", client);
    if (this->tv_sec == 0)
        setWakeup(time(NULL) + this->dbtimeout);
    msg.pdebug(D_CON, "Nächstes Aufräumen %s", Message::timestamp(this->tv_sec).c_str());
}


void DbHttpAnalyse::disconnect(int client)
{
#ifdef PTHREAD
    pthread_mutex_lock(&cl_mutex);
#endif
	del_client(client);
	HttpAnalyse::disconnect(client);
#ifdef PTHREAD
    pthread_mutex_unlock(&cl_mutex);
#endif
}

void DbHttpAnalyse::timeout(long sec, long usec, long w_sec, long w_usec)
{
	Clients::iterator c;
	int need_timeout = 0;

#ifdef PTHREAD
    pthread_mutex_lock(&cl_mutex);
#endif
	msg.pdebug(D_CON, "Starte aufräumen");
	c = clients.begin();

	while (c != clients.end() )
	{
		for (c = clients.begin(); c != clients.end(); ++c)
		{
			if (c->second.last_connect != 0)
			{
				if (c->second.last_connect + dbtimeout <= time(NULL) )
				{
					msg.pdebug(D_CON, "lösche client endgültig %s",
							c->first.c_str());
#ifdef PTHREAD
					pthread_mutex_lock(&c->second.mutex);
#endif
					delete c->second.db;
					clients.erase(c);
					break;
				}
				else
				{
					if (need_timeout == 0 || c->second.last_connect
							< need_timeout)
						need_timeout = c->second.last_connect;
				}
			}
		}
	}

	msg.pdebug(D_CON, "Beende aufräumen need_timeout %d", need_timeout);

	if (need_timeout == 0)
		setWakeup(0);
	else if (need_timeout + this->dbtimeout < time(NULL) + 60)
		setWakeup(time(NULL) + 60);
	else
		setWakeup(need_timeout + this->dbtimeout);

#ifdef PTHREAD
    pthread_mutex_unlock(&cl_mutex);
#endif
}

