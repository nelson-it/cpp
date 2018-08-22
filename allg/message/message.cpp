#if defined(__CYGWIN__) || defined(__MINGW32__)
#include <winsock2.h>
#endif

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <iconv.h>

#include <argument/argument.h>
#include "message.h"

#if defined(__CYGWIN__) || defined(__MINGW32__)
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#else
#include <errno.h>
#endif


#if 1
#define Pthread_mutex_lock(x,y)  pthread_mutex_lock(y);
#define Pthread_mutex_unlock(y)  pthread_mutex_unlock(y);
#else
#define Pthread_mutex_lock(x,y)  fprintf(stderr, "lock %s\n", x);\
		pthread_mutex_lock(y);
#define Pthread_mutex_unlock(y)  fprintf(stderr, "unlock\n");\
		pthread_mutex_unlock(y);
#endif

Message::MessageClients Message::msg_clients;

std::map<void*, Message::Param> Message::params;
pthread_mutex_t Message::mutex = PTHREAD_MUTEX_INITIALIZER;

FILE *Message::out = stderr;
std::string Message::logfile;

Message::Message(const char *id, int logonly)
{
	last_debuglevel = 100000;

	this->msg_typ = M_UNDEF;
	errorclass = 0;
	strncpy(this->id, id, sizeof(this->id));
	this->id[sizeof(this->id) - 1] = '\0';
	this->logonly = logonly;

}

std::string Message::timestamp(time_t t)
{
	char str[128];
	int l;
	struct tm tm;

	if ( t == 0 ) time(&t);
	l = strftime(str, sizeof(str), "%H:%M:%S", localtime_r( &t, &tm));
	str[l] = '\0';

	return str;
}

Message::Param *Message::p_getParam()
{
	Pthread_mutex_lock("getParam", &mutex);
	std::map<void *, Param>::iterator i;
	void *tid =  PTHREADID;

	if ( ( i = params.find(tid)) == params.end() )
	{
		Param p;
		p.argdebug = -1000000;
		p.debug = -1000000;
		p.msg_trans = NULL;
		p.prg_trans = NULL;
		params[tid] = p;

		i = params.find(tid);
	}

	return &i->second;
	return NULL;
}

void Message::setMsgtranslator(MessageTranslator *msg_trans)
{
	Param *p = p_getParam();
	p->msg_trans = msg_trans;
	Pthread_mutex_unlock(&mutex);
}

void Message::setPrgtranslator(MessageTranslator *prg_trans)
{
	Param *p = p_getParam();
	p->prg_trans = prg_trans;
	Pthread_mutex_unlock(&mutex);
}

int Message::getErrorfound()
{
	Param *p = p_getParam();
	int result = p->errorfound;
	Pthread_mutex_unlock(&mutex);
	return result;
}

int Message::getWarningfound()
{
	Param *p = p_getParam();
	int result = p->warningfound;
	Pthread_mutex_unlock(&mutex);
	return result;
}

void Message::clear()
{
	Param *p = p_getParam();
	p->warningfound = p->errorfound = 0;
	Pthread_mutex_unlock(&mutex);
}

std::string Message::getLang()
{
    std::string lang;
    MessageTranslator *msg_trans = p_getParam()->msg_trans;

    if ( msg_trans != NULL )
        lang = msg_trans->getLang();
    else
        lang = "de";

    Pthread_mutex_unlock(&mutex);
    return lang;
}

std::string Message::getRegion()
{
    std::string region;

    MessageTranslator *msg_trans = p_getParam()->msg_trans;
    if ( msg_trans != NULL )
        region = msg_trans->getRegion();
    else
        region = "CH";

    Pthread_mutex_unlock(&mutex);
    return region;
}

void Message::setLang(std::string lang)
{
    MessageTranslator *msg_trans = p_getParam()->msg_trans;
    if ( msg_trans != NULL )
        msg_trans->setLang(lang);

    Pthread_mutex_unlock(&mutex);
}

void Message::setRegion(std::string region)
{
    MessageTranslator *msg_trans = p_getParam()->msg_trans;
    if ( msg_trans != NULL )
        msg_trans->setRegion(region);

    Pthread_mutex_unlock(&mutex);
}

std::string Message::getDateformat()
{
    std::string str;

    MessageTranslator *msg_trans = p_getParam()->msg_trans;
    if ( msg_trans != NULL )
        str = msg_trans->getDateformat();
    else
        str = "%d.%m.%Y";

    Pthread_mutex_unlock(&mutex);
    return str;
}

std::string Message::getTimeformat()
{
    std::string str;

    MessageTranslator *msg_trans = p_getParam()->msg_trans;
    if ( msg_trans != NULL )
        str = msg_trans->getTimeformat();
    else
        str = "%H:%M:%S";

    Pthread_mutex_unlock(&mutex);
    return str;
}

std::string Message::getIntervalformat()
{
    std::string str;

    MessageTranslator *msg_trans = p_getParam()->msg_trans;
    if ( msg_trans != NULL )
        str = msg_trans->getIntervalformat();
    else
        str = "%02d:%02d:%02d";

    Pthread_mutex_unlock(&mutex);
    return str;
}

std::string Message::getTimesformat()
{
    std::string str;

    MessageTranslator *msg_trans = p_getParam()->msg_trans;
    if ( msg_trans != NULL )
        str = msg_trans->getTimesformat();
    else
        str = "%H:%M";

    Pthread_mutex_unlock(&mutex);
    return str;
}

std::string Message::getIntervalsformat()
{
    std::string str;

    MessageTranslator *msg_trans = p_getParam()->msg_trans;
    if ( msg_trans != NULL )
        str = msg_trans->getIntervalsformat();
    else
        str = "%02d:%02d";

    Pthread_mutex_unlock(&mutex);
    return str;
}



void Message::perror(int errorno, const char *str, ... )
{
	Param *p = p_getParam();
	void *tid = PTHREADID;

	MessageTranslator *prg_trans = p->prg_trans;
	MessageTranslator *msg_trans = p->msg_trans;

	p->errorfound++;
	Pthread_mutex_unlock(&mutex);

	va_list ap;
	va_start(ap, str);

	this->msg_typ = M_ERROR;

	fprintf(out, "%s E%04d: %6s ", timestamp().c_str(), errorclass + errorno, id);
	if ( prg_trans == NULL || ignore_lang)
		vfprintf(out, str, ap);
	else
		vfprintf(out, prg_trans->get(str, id).c_str(), ap);
	va_end(ap);

	if ( ! msg_clients.empty() )
	{
		MessageClients::iterator i;
		char s[10240];
		*s = '\0';
		va_start(ap, str);
		if ( errorno > 0 )
			sprintf(s, "%s ", id);
		if ( msg_trans == NULL || ignore_lang)
			vsnprintf(&s[strlen(s)], sizeof(s) - strlen(s), str, ap );
		else
			vsnprintf(&s[strlen(s)], sizeof(s) - strlen(s), msg_trans->get(str, id).c_str(), ap );
		s[sizeof(s) - 1] = '\0';

		for ( i=msg_clients.begin(); i != msg_clients.end(); ++i )
			if ( (*i)->tid == NULL || (*i)->tid == tid )
			{
			    if ( logonly )
			        (*i)->perror((char*)msg_trans->get("Fehler gefunden").c_str());
			    else
				    (*i)->perror(s);
			}
		va_end(ap);
	}

	fprintf(out,"\n");
	fflush(out);
	ignore_lang = 0;

}

void Message::pwarning(int errorno, const char *str, ... )
{
	Param *p = p_getParam();
	void *tid = PTHREADID;

	MessageTranslator *prg_trans = p->prg_trans;
	MessageTranslator *msg_trans = p->msg_trans;

	p->warningfound++;
	Pthread_mutex_unlock(&mutex);

	va_list ap;
	va_start(ap,str);

	this->msg_typ = M_WARNING;

	fprintf(out, "%s W%04d: %6s ", timestamp().c_str(), errorclass + errorno, id);
	if ( prg_trans == NULL || ignore_lang)
		vfprintf(out, str, ap);
	else
		vfprintf(out, prg_trans->get(str, id).c_str(), ap);

	va_end(ap);

	if ( ! msg_clients.empty() )
	{
		MessageClients::iterator i;
		char s[10240];
		*s = '\0';
		va_start(ap,str);
		if ( errorno > 0 )
			sprintf(s, "%s ", id);
		if ( msg_trans == NULL || ignore_lang)
			vsnprintf(&s[strlen(s)], sizeof(s) - strlen(s), str, ap );
		else
			vsnprintf(&s[strlen(s)], sizeof(s) - strlen(s), msg_trans->get(str, id).c_str(), ap );
		s[sizeof(s) - 1] = '\0';

		for ( i=msg_clients.begin(); i != msg_clients.end(); ++i )
			if ( (*i)->tid == NULL || (*i)->tid == tid )
			{
                if ( logonly )
                    (*i)->pwarning((char*)msg_trans->get("Warnung gefunden").c_str());
                else
				    (*i)->pwarning(s);
			}
		va_end(ap);
	}

	fprintf(out,"\n");
	fflush(out);
	ignore_lang = 0;

}

void Message::pmessage(int errorno, const char *str, ... )
{
	Param *p = p_getParam();
	void *tid = PTHREADID;

	MessageTranslator *prg_trans = p->prg_trans;
	MessageTranslator *msg_trans = p->msg_trans;
	Pthread_mutex_unlock(&mutex);

	va_list ap;
	va_start(ap,str);

	this->msg_typ = M_MESSAGE;

	if ( errorno > 0 )
		fprintf(out, "%s M%04d: %6s ", timestamp().c_str(), errorclass + errorno, id);
	if ( prg_trans == NULL || ignore_lang)
		vfprintf(out, str, ap);
	else
		vfprintf(out, prg_trans->get(str, id).c_str(), ap);

	va_end(ap);

	if ( logonly == 0 && ! msg_clients.empty() )
	{
		MessageClients::iterator i;
		char s[10240];
		*s = '\0';
		va_start(ap,str);
		if ( errorno > 0 )
			sprintf(s, "%s ", id);
		if ( msg_trans == NULL || ignore_lang)
			vsnprintf(&s[strlen(s)], sizeof(s) - strlen(s), str, ap );
		else
			vsnprintf(&s[strlen(s)], sizeof(s) - strlen(s), msg_trans->get(str, id).c_str(), ap );
		s[sizeof(s) - 1] = '\0';

		for ( i=msg_clients.begin(); i != msg_clients.end(); ++i )
			if ( (*i)->tid == NULL || (*i)->tid == tid )
			    (*i)->pmessage(s);
		va_end(ap);
	}

	fprintf(out,"\n");
	fflush(out);
	ignore_lang = 0;

}

void Message::pdebug(int debuglevel, const char *str, ... )
{
	Param *p = p_getParam();
	int debug = p->debug;
	Pthread_mutex_unlock(&mutex);
	va_list ap;

	this->msg_typ = M_DEBUG;
	last_debuglevel = debuglevel;

	if ( debug == -1000000 )
	{
		Argument a;
		debug = a["debug"];
		Pthread_mutex_lock("pdebug", &mutex);
		p->debug = p->argdebug = debug;
		Pthread_mutex_unlock(&mutex);
	}

	if (    ( debug >= 0 && debug < debuglevel )
			|| ( debug < 0 && debuglevel != -debug ))
	{
		return;
	}

	Pthread_mutex_lock("pdebug", &mutex);
	va_start(ap,str);
	fprintf(out, "%s D%04d: %6s ", timestamp().c_str(), debuglevel, id);
	vfprintf(out, str, ap);
	va_end(ap);
	fprintf(out,"\n");
	fflush(out);
	ignore_lang = 0;
	Pthread_mutex_unlock(&mutex);

}

void Message::wdebug(int debuglevel, const char *str, int length)
{
	Param *p = p_getParam();
	int debug = p->debug;
	Pthread_mutex_unlock(&mutex);
	char *c;

	this->msg_typ = M_DEBUG;
	last_debuglevel = debuglevel;

	if ( debug == -1000000 )
	{
		Argument a;
		debug = a["debug"];
		Pthread_mutex_lock("pdebug", &mutex);
		p->debug = p->argdebug = debug;
		Pthread_mutex_unlock(&mutex);
	}

	if (    ( debug >= 0 && debug < debuglevel )
			|| ( debug < 0 && debuglevel != -debug ))
	{
		return;
	}

	c = new char[length + 1];
	memcpy(c, str, length);
	c[length] = '\0';

	Pthread_mutex_lock("wdebug", &mutex);
	fprintf(out, "%s D%04d: %s ", timestamp().c_str(), debuglevel, c);
	fflush(out);
	delete c;
	ignore_lang = 0;
	Pthread_mutex_unlock(&mutex);
}

void Message::iline(const char *str, ... )
{
    Param *p = p_getParam();
    void *tid = PTHREADID;

    int debug = p->debug;
    Pthread_mutex_unlock(&mutex);
    va_list ap;

    if ( this->msg_typ == M_DEBUG && debug < last_debuglevel )
    {
        Pthread_mutex_unlock(&mutex);
        return;
    }

    va_start(ap,str);
    fprintf(out, "                ");
    vfprintf(out, str, ap);

    va_end(ap);

    if ( ! logonly && this->msg_typ != M_DEBUG && this->msg_typ != M_UNDEF && ! msg_clients.empty() )
    {
        MessageClients::iterator i;
        char s[10240];
        unsigned int j;
        va_start(ap,str);
        for ( j=0; j<strlen(id); j++) s[j] = ' ';
        s[j++] = ' ';
        s[j] = '\0';

        vsnprintf(&s[strlen(s)], sizeof(s) - strlen(s), str, ap );
        s[sizeof(s) - 1] = '\0';

        for ( i=msg_clients.begin(); i != msg_clients.end(); ++i )
        {
            if ( (*i)->tid == NULL || (*i)->tid == tid )
                (*i)->pline(s);
        }
        va_end(ap);
    }

    fprintf(out,"\n");
    fflush(out);
    ignore_lang = 0;

}
void Message::line(const char *str, ... )
{
    Param *p = p_getParam();
    void *tid = PTHREADID;

    int debug = p->debug;
    MessageTranslator *prg_trans = p->prg_trans;
    MessageTranslator *msg_trans = p->msg_trans;
    Pthread_mutex_unlock(&mutex);
    va_list ap;

    if ( this->msg_typ == M_DEBUG && debug < last_debuglevel )
    {
        Pthread_mutex_unlock(&mutex);
        return;
    }

    va_start(ap,str);
    fprintf(out, "                ");

    if ( this->msg_typ == M_DEBUG || prg_trans == NULL || ignore_lang)
        vfprintf(out, str, ap);
    else
        vfprintf(out, prg_trans->get(str, id).c_str(), ap);

    va_end(ap);

    if ( ! logonly && this->msg_typ != M_DEBUG && this->msg_typ != M_UNDEF && ! msg_clients.empty() )
    {
        MessageClients::iterator i;
        char s[10240];
        unsigned int j;
        va_start(ap,str);
        for ( j=0; j<strlen(id); j++) s[j] = ' ';
        s[j++] = ' ';
        s[j] = '\0';

        if ( msg_trans == NULL || ignore_lang)
            vsnprintf(&s[strlen(s)], sizeof(s) - strlen(s), str, ap );
        else
            vsnprintf(&s[strlen(s)], sizeof(s) - strlen(s), msg_trans->get(str, id).c_str(), ap );
        s[sizeof(s) - 1] = '\0';

        for ( i=msg_clients.begin(); i != msg_clients.end(); ++i )
        {
            if ( (*i)->tid == NULL || (*i)->tid == tid )
                (*i)->pline(s);
        }
        va_end(ap);
    }

    fprintf(out,"\n");
    fflush(out);
    ignore_lang = 0;

}

void Message::ptext(const char *buffer, int length)
{
	fprintf(out, "%s Text:\n", timestamp().c_str());
    fwrite(buffer, length, 1, out );
    fprintf(out, "\n");
    fflush(out);
}

std::string Message::get( std::string str, int prog )
{
	Param *p = p_getParam();
	MessageTranslator *prg_trans = p->prg_trans;
	MessageTranslator *msg_trans = p->msg_trans;
	Pthread_mutex_unlock(&mutex);

	if ( prog == 0 && msg_trans != NULL )
	{
		return msg_trans->get(str, id);
	}
	else if ( prog == 1 && prg_trans != NULL )
	{
		return prg_trans->get(str, id);
	}
	else
	{
		return str;
	}

}

std::string Message::getSystemerror(int errornumber )
{
	Pthread_mutex_lock("system",&mutex);
	std::string ret;

#if defined(__CYGWIN__) || defined(__MINGW32__)
    iconv_t iv;
    char *inbuf, *outbuf, *ci, *co;
    size_t innum,outnum;

	FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			errornumber,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &inbuf,
			0,
			NULL
	);

    ci = inbuf;
    innum = strlen(inbuf);

    outbuf = co  = new char[innum * 4];
    outnum = ( innum - 1);

    iv = iconv_open("utf-8//TRANSLIT", "iso-8859-1");
    iconv (iv, &ci, &innum, &co, &outnum);
    iconv_close(iv);
    *co = '\0';

    ret = outbuf;

	LocalFree(inbuf);
	delete outbuf;

	Pthread_mutex_unlock(&mutex);
	return ret;
#else

	ret = strerror(errno);
	Pthread_mutex_unlock(&mutex);
	return ret;
#endif

}

void Message::mklog(std::string filename)
{
	Pthread_mutex_unlock(&mutex);
	logfile = filename;
	if ( out != stderr )
		fclose(out);

	out = fopen(filename.c_str(), "w");
	if ( out == NULL )
	{
		out = stderr;

		Pthread_mutex_unlock(&mutex);
		perror(1, "Datei %s konnte nicht geöffnet werden", filename.c_str());
		return;
	}

	Pthread_mutex_unlock(&mutex);
}

void Message::trunclog()
{
	if ( logfile != "" )
		mklog(logfile);
}
