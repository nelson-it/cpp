#ifndef messages_mne
#define messages_mne

#include <string>
#include <vector>
#include <algorithm>

#ifdef PTHREAD
#include <map>
#include <pthread.h>
#define PTHREADID (void*)pthread_self()
#endif

#if defined(__MINGW32__) || defined(__CYGWIN__)
#define DEF_LOCALE  "German"
#else
#define DEF_LOCALE "de.DE.utf8"
#endif

class MessageTranslator
{
public:
	virtual std::string get(const char *str, const char *kategorie = "") = 0;
	virtual std::string get(std::string str, const char *kategorie = "") = 0;

    virtual void setLang(std::string lang) = 0;
    virtual void setRegion(std::string region) = 0;

    virtual std::string getLang() = 0;
    virtual std::string getRegion() = 0;

    virtual std::string getDateformat(){ return "%d.%m.%Y";  }
    virtual std::string getTimeformat() { return "%H:%M:%S"; }
    virtual std::string getIntervalformat() { return "%02d:%02d:%02d"; }
    virtual std::string getTimesformat() { return "%H:%M"; }
    virtual std::string getIntervalsformat() { return "%02d:%02d"; }

    virtual void clear_cache() = 0;
};

class Message
{
public:
	class MessageClient
	{
		friend class Message;
	protected:
		void *tid;
	public:
		MessageClient()
		{
			tid = NULL;
		}
		void setThreadonly()
		{
		    tid = PTHREADID;
		}

		virtual void perror(char *str) = 0;
		virtual void pwarning(char *str) = 0;
		virtual void pmessage(char *str) = 0;
		virtual void pline(char *str ) = 0;
	};

private:
	typedef std::vector<MessageClient *> MessageClients;
	static MessageClients msg_clients;

	enum MESSAGE_TYPE
	{
		M_UNDEF, M_ERROR, M_WARNING, M_MESSAGE, M_DEBUG, M_LINE
	};

#ifdef PTHREAD
public:
	class Param
	{
	public:
		int argdebug;
		int debug;
		int errorfound;
		int warningfound;

		MessageTranslator *prg_trans;
		MessageTranslator *msg_trans;
	    Param()
		{
	        argdebug = 0;
	        debug = 0;
	        errorfound = 0;
	        warningfound = 0;
		};
	};
private:
	static std::map<void *, Param> params;
	static pthread_mutex_t mutex;
	Param *p_getParam();

#else

	static int argdebug;
	static int debug;
	static int errorfound;
	static int warningfound;

	static MessageTranslator *prg_trans;
	static MessageTranslator *msg_trans;

#endif

       static std::string logfile;
       static FILE *out;

        int errorclass;
	int msg_typ;
	int last_debuglevel;

	char id[32];
	int logonly;

public:
	int ignore_lang;

	Message(const char *id, int logonly = 0);

	static std::string timestamp(time_t t = 0);

	void perror(int errorno, const char *str, ...);
	void pwarning(int errorno, const char *str, ...);
	void pmessage(int errorno, const char *str, ...);
	void pdebug(int debuglevel, const char *str, ...);
	void wdebug(int debuglevel, const char *str, int length);

	void line(const char *str, ...);

	int getErrorfound();
	int getWarningfound();
    void clear();

	void setMsgtranslator(MessageTranslator *msg_trans);
	void setPrgtranslator(MessageTranslator *prg_trans);

	std::string get(std::string str, int prog = 0);

    void setLang(std::string lang);
    void setRegion(std::string region);

    std::string getLang();
    std::string getRegion();

    std::string getDateformat();
    std::string getTimeformat();
    std::string getIntervalformat();
    std::string getTimesformat();
    std::string getIntervalsformat();

	void start_debug(int i)
	{
#ifdef PTHREAD
		params[PTHREADID].debug = i;
#else
		debug = i;
#endif
	}
	void stop_debug()
	{
#ifdef PTHREAD

		params[PTHREADID].debug = params[PTHREADID].argdebug;
#else
                debug = argdebug;

#endif
	}

	std::string getSystemerror(int errornumber);

	void mklog(std::string filename);
	void trunclog();

#ifdef PTHREAD
	void clear_tread()
	{
		std::map<void *, Param>::iterator i;
		if ((i = params.find(PTHREADID)) != params.end())
			params.erase(i);
	}
#endif

	void add_msgclient(MessageClient *msg_client)
	{
#ifdef PTHREAD
		pthread_mutex_lock(&mutex);
#endif
		msg_clients.push_back(msg_client);
		msg_client->tid = NULL;
#ifdef PTHREAD
		pthread_mutex_unlock(&mutex);
#endif

	}
	void del_msgclient(MessageClient *msg_client)
	{
#ifdef PTHREAD
		pthread_mutex_lock(&mutex);
#endif

		MessageClients::iterator i;
		while ((i = find(msg_clients.begin(), msg_clients.end(), msg_client))
				!= msg_clients.end())
			msg_clients.erase(i);

#ifdef PTHREAD
		pthread_mutex_unlock(&mutex);
#endif
	}

};

#endif /* messages_mne */
