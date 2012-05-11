#ifndef dbhttp_analyse_mne
#define dbhttp_analyse_mne

#include <string>
#include <map>

#include <embedweb/http_analyse.h>
#include <ipc/timeout.h>
#include <message/message.h>

class Database;
class DbHttp;

class DbHttpAnalyse : public HttpAnalyse, public TimeoutClient
{
public:
	class Client
	{
	    Message msg;
public:

#ifdef PTHREAD
		pthread_mutex_t mutex;
#endif

		unsigned int host;
		int          browser;
		std::string  user_agent;
		std::string  user;
		std::string  passwd;

		Database     *db;
		time_t       last_connect;

        typedef std::map<std::string, std::string> Userprefs;
        Userprefs userprefs;

		Client()
            :msg("DbHttpAnalyse::Client")
		{
#ifdef PTHREAD
			pthread_mutex_init(&mutex,NULL);
#endif

			host = 0;
			browser = 0;
			last_connect = 0;

            userprefs["language"] = "de";
            userprefs["region"] = "CH";
			userprefs["countrycarcode"] = "CH";

			db = NULL;
		};

		std::string getUserprefs(std::string name)
		{
		    std::map<std::string, std::string>::iterator i;
		    if ( ( i = userprefs.find(name)) != userprefs.end() )
		        return i->second;
		    msg.pwarning(1, "Usereinstellung f�r <%s> ist nicht vorhanden", name.c_str());
		    return "";
		}
	};

private:

	enum DEBUG_TYPES
	{
		D_CON = 1,
		D_CLIENT = 3,
		D_MUTEX = 6
	};

#ifdef PTHREAD
	pthread_mutex_t cl_mutex;
#endif

	typedef std::map<std::string, Client> Clients;

	Message msg;
	ServerSocket *s;

	Database *db;
	Clients clients;

	int user_count;
	time_t dbtimeout;
	std::string realm;

	void check_user(HttpHeader *h);
	void del_client(unsigned int clientid);


protected:
#ifdef PTHREAD
	void releaseHeader(HttpHeader *h)
	{
		Clients::iterator i;
		msg.pdebug(D_MUTEX, "mutex release %s", h->id.c_str());
		if ( ( i = clients.find(h->id) ) != clients.end() )
		{
			pthread_mutex_unlock(&i->second.mutex);
		}
		HttpAnalyse::releaseHeader(h);
	}
#endif


    void setUserprefs(Client *cl);

public:
	DbHttpAnalyse(ServerSocket *s, Database *db);
	virtual ~DbHttpAnalyse();

	void timeout(long sec, long usec, long w_sec, long w_usec );
	virtual void disconnect( int client );


	Client *getClient(std::string cookie)
	{
	    Clients::iterator i;
	    if ( ( i = clients.find(cookie) ) == clients.end() )
	    {
	        return NULL;
	    }
	    else
	    {
#ifdef PTHREAD
	        msg.pdebug(D_MUTEX, "mutex lock %s", cookie.c_str());
	        pthread_mutex_lock(&(i->second.mutex));
#endif

	        return &(i->second);
	    }
	}
};

#endif /* dbhttp_analyse_mne */
