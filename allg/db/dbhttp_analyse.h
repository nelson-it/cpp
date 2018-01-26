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
		int is_unlock;
#endif

		unsigned int host;
		int          browser;
		std::string  user_agent;
		std::string  user;
		std::string  passwd;
		std::string  base;
		std::string  cookie;

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
			is_unlock = 0;

            userprefs["language"] = "de";
            userprefs["region"] = "CH";
			userprefs["countrycarcode"] = "CH";

			db = NULL;
		};

		~Client()
		{
            #ifdef PTHREAD
		    pthread_mutex_unlock(&mutex);
		    pthread_mutex_destroy(&mutex);
            #endif
		}

        void unlock()
        {
             #ifdef PTHREAD
             msg.pdebug(1, "try unlock %s", cookie.c_str());
            if ( is_unlock )
                 return;
             msg.pdebug(1, "unlock %s", cookie.c_str());
             is_unlock = 1;
             pthread_mutex_unlock(&mutex);
             #endif
        }

        void lock()
        {
             #ifdef PTHREAD
             msg.pdebug(1, "lock %s", cookie.c_str());
             pthread_mutex_lock(&mutex);
             is_unlock = 0;
             #endif
        }

		std::string getUserprefs(std::string name)
		{
		    std::map<std::string, std::string>::iterator i;
		    if ( ( i = userprefs.find(name)) != userprefs.end() )
		        return i->second;
		    msg.pwarning(1, "Usereinstellung für <%s> ist nicht vorhanden", name.c_str());
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
	void lock() { pthread_mutex_lock(&cl_mutex); }
	void unlock() { pthread_mutex_unlock(&cl_mutex); }
#endif

	typedef std::map<std::string, Client> Clients;

	Message msg;
	ServerSocket *s;

	Database *db;
	Clients clients;

	int user_count;
	time_t dbtimeout;
	std::string realm;
	std::string cookieid;

	void check_user(HttpHeader *h);
	void del_client(unsigned int clientid);


protected:
#ifdef PTHREAD
	void releaseHeader(HttpHeader *h)
	{
	    Clients::iterator i;
	    if ( ( i = clients.find(h->id) ) != clients.end() )
	    {
	        i->second.unlock();
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
	        i->second.lock();
	        return &(i->second);
	    }
	}

	void read_datadir();
};

#endif /* dbhttp_analyse_mne */
