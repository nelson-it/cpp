#ifdef PTHREAD
#include <pthread.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <sec_api/stdio_s.h>
#endif
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <utils/gettimeofday.h>
#include <utils/tostring.h>
#include <utils/php_exec.h>
#include <crypt/base64.h>
#include <argument/argument.h>

#include "http.h"
#include "http_analyse.h"
#include "http_provider.h"
#include "http_map.h"

Http::Http(ServerSocket *s, HttpAnalyse *analyse, int register_thread) :
	msg("Http")
{

	Argument a;
	std::string mstr;
	CsList mlist;
	CsList mele;
	unsigned int i;

	no_cache = a["EmbedwebHttpNocache"];

	mlist.setString((char *)a["EmbedwebHttpMaps"], ':');
	for (i=0; i<mlist.size(); i++)
	{
		mele.setString(mlist[i], '!');
		maps.push_back(new HttpMap(this, mele[0], mele[1], mele[2]));
	}

	status_str[100] = "Continue";
	status_str[101] = "Switching Protocols";
	status_str[200] = "OK";
	status_str[201] = "Created";
	status_str[202] = "Accepted";
	status_str[203] = "Non-Authoritative Information";
	status_str[204] = "No Content";
	status_str[205] = "Reset Content";
	status_str[206] = "Partial Content";
	status_str[300] = "Multiple Choices";
	status_str[301] = "Moved Permanently";
	status_str[302] = "Found";
	status_str[303] = "See Other";
	status_str[304] = "Not Modified";
	status_str[305] = "Use Proxy";
	status_str[307] = "Temporary Redirect";
	status_str[400] = "Bad Request";
	status_str[401] = "Unauthorized";
	status_str[402] = "Payment Required";
	status_str[403] = "Forbidden";
	status_str[404] = "Not Found";
	status_str[405] = "Method Not Allowed";
	status_str[406] = "Not Acceptable";
	status_str[407] = "Proxy Authentication Required";
	status_str[408] = "Request Time-out";
	status_str[409] = "Conflict";
	status_str[410] = "Gone";
	status_str[411] = "Length Required";
	status_str[412] = "Precondition Failed";
	status_str[413] = "Request Entity Too Large";
	status_str[414] = "Request-URI Too Large";
	status_str[415] = "Unsupported Media Type";
	status_str[416] = "Requested range not satisfiable";
	status_str[417] = "Expectation Failed";
	status_str[500] = "Internal Server Error";
	status_str[501] = "Not Implemented";
	status_str[502] = "Bad Gateway";
	status_str[503] = "Service Unavailable";
	status_str[504] = "Gateway Time-out";
	status_str[505] = "HTTP Version not supported";

    // Normal Meldung falls nicht überladen
	// ====================================
	meldungen[200]
			= "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//DE\">"
				"<html>"
				"<head>"
				"<title>M Nelson Embedded Http Server</title>"
				"<meta http-equiv=\"Content-Type\""
				" content=\"text/html; charset=UTF-8\">"
				"</head>"
				"<body>"
				"Request: #request# ist in Ordnung"
				"</body>"
				"</html>";

    // Benötige authorisation
    // ======================
	meldungen[401]
			= "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//DE\">"
				"<html>"
				"<head>"
				"<title>M Nelson Embedded Http Server 401</title>"
				"<meta http-equiv=\"Content-Type\""
				" content=\"text/html; charset=UTF-8\">"
				"</head>"
				"<body>"
				"Login ist fehlgeschlagen"
				"</body>"
				"</html>";

    // Anmeldung gescheitert
    // =====================
	meldungen[403]
			= "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//DE\">"
				"<html>"
				"<head>"
				"<title>M Nelson Embedded Http Server 403</title>"
				"<meta http-equiv=\"Content-Type\""
				" content=\"text/html; charset=UTF-8\">"
				"</head>"
				"<body>"
				"Login ist fehlgeschlagen"
				"</body>"
				"</html>";
	status_str[403] = "Forbidden";

	// Document wurde nicht gefunden
	// =====================
	meldungen[404]
			= "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//DE\">"
				"<html>"
				"<head>"
				"<title>M Nelson Embedded Http Server 404</title>"
				"<meta http-equiv=\"Content-Type\""
				" content=\"text/html; charset=UTF-8\">"
				"</head>"
				"<body>"
				"Die angefragte Seite: #request# wurde nicht gefunden"
				"</body>"
				"</html>";

	this->s = s;
	this->analyse = analyse;

	if (register_thread)
		analyse->add_http(this);
}

Http::~Http()
{
	unsigned int i;
	analyse->del_http(this);

	for (i=0; i<maps.size(); i++)
		delete maps[i];
}
std::string Http::get_meldungtext(int status)
{
    FILE *fp;
    std::string str;
    char buffer[102400];
    int num;
	struct stat s;
	unsigned int i;
	std::vector<std::string> serverpath;

    sprintf(buffer, "%d", status);

    serverpath = analyse->getServerpath();
    for (i=0; i<serverpath.size(); i++)
	{
		str = serverpath[i] + "/" + act_h->dirname + "/" + act_h->filename;
		if (stat(str.c_str(), &s) == 0 ) break;
	}

    if ( (fp = fopen(str.c_str(), "rb") ) == NULL)
    {
        if ( meldungen.find(status) != meldungen.end())
            return meldungen[status];
        else
            return (std::string)("Status: ") + buffer;
    }
    else
    {
        num = fread(buffer, 1, sizeof(buffer) - 1, fp);
        buffer[num] = '\0';
        return buffer;
    }
}

HttpProvider *Http::find_provider(Provider *p)
{
	Provider::iterator act_p;

	for (act_p = p->begin(); act_p != p->end(); ++act_p)
		if ( act_h->dirname.find(act_p->first) == 1 && ( act_h->dirname[act_p->first.length() + 1] == '\0' || act_h->dirname[act_p->first.length() + 1] == '/'))
			break;

	if (act_p != p->end() )
	{
		act_h->providerpath = act_p->first;
		if (act_h->dirname.size() == (act_p->first.size() + 1 ))
			act_h->dirname = "/";
		else
			act_h->dirname = act_h->dirname.substr(act_p->first.size() + 1);

		msg.pdebug(D_HTTP, "Provider: %s dirname: %s filename: %s",
				act_p->first.c_str(), act_h->dirname.c_str(),
				act_h->filename.c_str());
		return act_p->second;
	}
	else
	{
		return NULL;
	}
}

std::string Http::mkmap(std::string filename)
{
	Maps::iterator act_m;
    unsigned int i;
	struct stat s;

	for (act_m = maps.begin(); act_m != maps.end(); ++act_m)
		if (filename.find((*act_m)->getPath()) == 1)
			return (*act_m)->mkfilename(act_h, filename);

	for (i=0; i<act_h->serverpath.size(); i++)
		if (stat((act_h->serverpath[i] + filename).c_str(), &s) == 0)
			return act_h->serverpath[i] + filename;

	return "";
}

void Http::make_answer()
{
	FILE *file;
	std::string str;
	HttpProvider *p;
    unsigned int i;
	struct stat s;

	if (act_h->protokoll == "")
	{
		act_h->status = 404;
		act_h->age = 0;
		//act_h->content_type = "text/html";
		str = this->get_meldungtext(act_h->status);
		std::string::size_type pos = str.find("#request#");
		if (pos != std::string::npos)
			str.replace(pos, 9, act_h->dirname + "/" + act_h->filename);

		fwrite(str.c_str(), str.size(), 1, act_h->content);
		return;
	}

	if (act_h->status != 404)
	{
		act_h->age = 0;
	    str = this->get_meldungtext(act_h->status);

		std::string::size_type pos = str.find("#request#");
		if (pos != std::string::npos)
			str.replace(pos, 9, act_h->dirname + "/" + act_h->filename);

		fwrite(str.c_str(), str.size(), 1, act_h->content);
		return;
	}

	if ( (p = find_provider(&provider)) != NULL)
	{
		if ( !p->request(act_h) )
		{
			act_h->status = 404;
			act_h->age = 0;
			if (act_h->filename.find(".xml") == (act_h->filename.size() - 4))
			{
				msg.perror(E_NOTFOUND, "Provider %s unterstützt %s/%s nicht",
						act_h->providerpath.c_str(), act_h->dirname.c_str(), act_h->filename.c_str());
				act_h->content_type = "text/xml";
				fprintf(act_h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result>", act_h->charset.c_str());
				fprintf(act_h->content, "<body>error</body>");
			}
			else
			{
		        str = this->get_meldungtext(act_h->status);
				std::string::size_type pos = str.find("#request#");
				if (pos != std::string::npos)
					str.replace(pos, 9, act_h->dirname + "/" + act_h->filename);

				fwrite(str.c_str(), str.size(), 1, act_h->content);
			}
		}

		if (act_h->content_type.find("text") == 0 && act_h->translate != 0)
		{
			http_translate.make_answer(act_h, NULL);
			act_h->translate = 0;
		}

		if (act_h->status != 1000)
			return;
	}

	for (i=0; i<act_h->serverpath.size(); i++)
	{
		str = act_h->serverpath[i] + "/" + act_h->dirname + "/" + act_h->filename;
		if (stat(str.c_str(), &s) == 0 ) break;
	}

	if ( (file = fopen(str.c_str(), "rb") ) == NULL)
	{
	    if ( act_h->vars["ignore_notfound"] == "" )
            msg.perror(E_NOTFOUND, "Kann Datei %s/%s nicht finden", act_h->dirname.c_str(), act_h->filename.c_str());
		act_h->status = 404;
		act_h->age = 0;
		if (act_h->filename.find(".xml") == (act_h->filename.size() - 4))
		{
			act_h->content_type = "text/xml";
			fprintf(act_h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result>", act_h->charset.c_str());
			fprintf(act_h->content, "<body>error</body>");
		}
		else
		{
		    act_h->content_type = "text/html";
            str = this->get_meldungtext(act_h->status);
		    std::string::size_type pos = str.find("#request#");
		    if (pos != std::string::npos)
			    str.replace(pos, 9, act_h->dirname + "/" + act_h->filename);

		    fwrite(str.c_str(), str.size(), 1, act_h->content);
		    act_h->error_messages.clear();
	    }
	}
	else
	{
		if (act_h->status != 1000)
			act_h->age = 3600;
		act_h->status = 200;

		if ( act_h->setstatus  )
		    act_h->status = act_h->setstatus;

        if (act_h->filename.find(".php") == (act_h->filename.size() - 4 ))
        {
            fclose(file);
            file = 0;
            PhpExec(str, act_h);
        }

		if (act_h->content_type.find("text") == 0)
		{
			http_translate.make_answer(act_h, file);
			act_h->translate = 0;
		}
		else if (file != NULL)
		{
			fclose(act_h->content);
			act_h->content = file;
			fseek(file, 0, SEEK_END);
			file = NULL;
		}

		if (file != NULL)
			fclose(file);
	}
}

void Http::write_header()
{
	HttpHeader::SetCookies::iterator i;
    int status;
    unsigned int count;
	char buffer[10240];

	if (act_h->content_length != 0)
		msg.pwarning(W_CONTENTLENGTH, "Content-Length ist schon gesetzt %s %s",
				act_h->dirname.c_str(), act_h->filename.c_str());

	if (act_h->content != NULL)
	{
		act_h->content_length = ftell(act_h->content);
		if ( act_h->content_length < 0 )
		{
		    msg.perror(E_CONTENTLENGTH, "Kann Content-Length nicht ermitteln - versuche File neu zu öffenen");
		    fclose(act_h->content);
		    act_h->content = fopen(act_h->content_filename.c_str(), "rb+");
		    fseek(act_h->content, 0, SEEK_END);
   		    act_h->content_length = ftell(act_h->content);
		    if ( act_h->content_length < 0 ) act_h->content_length = 0;
		}
		fseek(act_h->content, 0, SEEK_SET);
	}
	else
	{
		act_h->content_length = 0;
	}

	status = act_h->status;
	if (act_h->status < 0)
	{
		HttpVars::Vars::iterator v;
        status = -status;
		msg.perror(E_WRITEHEADER, "Headerdaten:");
		msg.line("%s: %s", msg.get("Type").c_str(), act_h->typ.c_str());
		msg.line("%s: %s", msg.get("Dirname").c_str(), act_h->dirname.c_str());
		msg.line("%s: %s", msg.get("Filename").c_str(), act_h->filename.c_str());
		msg.line("%s: %s", msg.get("Protokoll").c_str(), act_h->protokoll.c_str());
		msg.line("%s: %s", msg.get("Hostname").c_str(), act_h->hostname.c_str());
		msg.line("%s: %d", msg.get("Browser").c_str(), act_h->browser);
		//msg.line("%s: %s", msg.get("Version").c_str(), act_h->version.c_str());
		msg.line("%s: %s", msg.get("User").c_str(), act_h->user.c_str());

		v = act_h->vars.p_getVars()->begin();
		for (; v != act_h->vars.p_getVars()->end(); ++v)
			msg.line("%s: %s", v->first.c_str(), v->second.c_str());

	}
	msg.pdebug(D_HTTP, "schreibe header - status \"%d\",\"%s\" \"%s\" %d",
			status, status_str[status].c_str(),
			act_h->content_type.c_str(), act_h->content_length);

	sprintf(buffer, "HTTP/1.1 %d %s\r\n", status,
			status_str[status].c_str());
	s->write(act_h->client, buffer, strlen(buffer));

	if ( status == 401 )
	{
		msg.pdebug(D_HTTP, "fordere Passwort an");
		if ( act_h->realm.empty() )
			sprintf(buffer, "WWW-Authenticate: Basic realm=\"%d\"\r\n",
					((int)time(0)));
		else
			sprintf(buffer, "WWW-Authenticate: Basic realm=\"%s\"\r\n",
					act_h->realm.c_str());
		s->write(act_h->client, buffer, strlen(buffer));
		buffer[strlen(buffer) - 2 ] = '\0';
		msg.pdebug(D_CON, "fordere Passwort an %s", buffer);
	}

	sprintf(buffer, "Server: M Nelson Embedded Http Server 0.9\r\n");
	s->write(act_h->client, buffer, strlen(buffer));

	if (act_h->browser != HttpHeader::B_IE && (no_cache || act_h->age == 0))
	{
		msg.pdebug(D_HTTP, "Cache_Control: no-store");
		sprintf(buffer, "Cache-Control: no-store\r\n");
	}
	else
	{
		if (no_cache)
			act_h->age = -1;
        msg.pdebug(D_HTTP, "Cache_Control: no-store");
        sprintf(buffer, "Cache-Control: no-store\r\n");

	}

	s->write(act_h->client, buffer, strlen(buffer));

	if (act_h->connection)
	{
		sprintf(buffer, "Connection: Keep-Alive\r\n");
		s->write(act_h->client, buffer, strlen(buffer));

		sprintf(buffer, "Keep-Alive: timeout=300, max=100000\r\n");
		s->write(act_h->client, buffer, strlen(buffer));
	}
	else
	{
		sprintf(buffer, "Connection: Close\r\n");
		s->write(act_h->client, buffer, strlen(buffer));

	}

	for (i = act_h->set_cookies.begin(); i != act_h->set_cookies.end(); ++i)
	{
		sprintf(buffer, "Set-Cookie: %s=%s; path=/\r\n", i->first.c_str(),
				i->second.c_str());
		s->write(act_h->client, buffer, strlen(buffer));
	}

	sprintf(buffer, "Accpet-Ranges: bytes\r\n");
	s->write(act_h->client, buffer, strlen(buffer));

	sprintf(buffer, "Accpet-Charset=\"utf-8\"\r\n");
	s->write(act_h->client, buffer, strlen(buffer));

	sprintf(buffer, "Content-Length: %ld\r\n", act_h->content_length);
	s->write(act_h->client, buffer, strlen(buffer));

	sprintf(buffer, "Content-Type: %s; charset=%s\r\n",
			act_h->content_type.c_str(), act_h->charset.c_str());
	s->write(act_h->client, buffer, strlen(buffer));

	for ( count = 0; count < act_h->extra_header.size(); ++count )
	{
	    sprintf(buffer, "%s\r\n", act_h->extra_header[count].c_str());
	    s->write(act_h->client, buffer, strlen(buffer));
	}

	if (status >= 300 && status < 400 )
	{
		msg.pdebug(D_HTTP, "redirect");

		if ( act_h->location == "" )
		{
		    msg.pdebug(D_HTTP, "Location: https://%s\r\n", act_h->hostname.c_str());
		    sprintf(buffer, "Location: https://%s\r\n", act_h->hostname.c_str());
		    s->write(act_h->client, buffer, strlen(buffer));
		    sprintf(buffer, "Content-Location: https://%s\r\n", act_h->hostname.c_str());
		    s->write(act_h->client, buffer, strlen(buffer));
		}
		else
		{
		    msg.pdebug(D_HTTP, "Location: %s\r\n", act_h->location.c_str());
		    sprintf(buffer, "Location: %s\r\n", act_h->location.c_str());
		    s->write(act_h->client, buffer, strlen(buffer));
		    sprintf(buffer, "Content-Location: %s\r\n", act_h->location.c_str());
		    s->write(act_h->client, buffer, strlen(buffer));

		}
	}
	sprintf(buffer, "\r\n");
	s->write(act_h->client, buffer, strlen(buffer));

}

void Http::write_trailer()
{
	msg.pdebug(D_HTTP, "write trailer - status \"%d\",\"%s\"",
			act_h->connection, act_h->connection ? "behalten" : "schliessen");

	if ( !act_h->connection)
		s->close(act_h->client);
}

void Http::send()
{
	make_answer();
	if ( !act_h->error_messages.empty() && act_h->content != NULL)
	{
		unsigned int i;

		if ( act_h->content_type == "text/html")
		{
			for (i = 0; i< act_h->error_messages.size(); ++i)
				fprintf(act_h->content, "%s\n", act_h->error_messages[i].c_str());
		}
		else if ( act_h->content_type == "text/plain")
		{
			for (i = 0; i< act_h->error_messages.size(); ++i)
				fprintf(act_h->content, "%s\n", act_h->error_messages[i].c_str());
		}
		else if (act_h->content_type == "text/xml")
		{
			fprintf(act_h->content, "<error>\n");
			for (i = 0; i< act_h->error_messages.size(); ++i)
				fprintf(act_h->content, "%s\n", act_h->error_messages[i].c_str());
			fprintf(act_h->content, "\n</error>");
		}
	}

	if ( act_h->content_type == "text/xml")
		fprintf(act_h->content,"</result>");

	write_header();
	if (act_h->content != NULL && act_h->content_length > 0 )
		s->write(act_h->client, act_h->content, act_h->content_length);
	write_trailer();

	s->flush(act_h->client);

	if (act_h->content != NULL)
	{
		fclose(act_h->content);
		act_h->content = NULL;
	}

}

void Http::get(HttpHeader *h)
{

	struct timeval t1, t2;
	long diff;

	gettimeofday(&t1, NULL);

	act_h = h;
#if defined(__MINGW32__) || defined(__CYGWIN__)
	char filename[512];
	*filename = '\0';
	if ( getenv ("TEMP") != NULL)
	{
		strncpy(filename, getenv("TEMP"), sizeof(filename) -1 );
		strncat(filename, "\\HttpXXXXXX", sizeof(filename) - strlen(filename) - 1);
	}
	_mktemp_s(filename, strlen(filename) + 1);
	filename[sizeof(filename) - 1] = '\0';
#else
	char str[32];
	strcpy(str, "/tmp/HttpXXXXXX");
	char *filename = mktemp(str);
#endif

	msg.add_msgclient(this);
	setThreadonly();
	msg.pdebug(D_HEADER, "tempfile: %s", filename);

	if ( (act_h->content = fopen(filename, "wb+")) == NULL)
	{
		msg.perror(E_TEMPFILE, "Kann temporäre Datei <%s> nicht öffnen", filename);

		s->close(act_h->client);
		return;
	}
	act_h->content_filename = filename;
#if defined(__MINGW32__) || defined(__CYGWIN__)
	if ( ! SetHandleInformation((HANDLE)_get_osfhandle(fileno(h->content)), HANDLE_FLAG_INHERIT, 0) )
		msg.pwarning(E_TEMPFILE, "SetHandleInformation schlug fehl");
#endif

	send();

	msg.del_msgclient(this);
#if defined(__MINGW32__) || defined(__CYGWIN__)
	DeleteFile(filename);
#else
	unlink(filename);
#endif
	act_h = NULL;

	gettimeofday(&t2, NULL);
	diff = (t2.tv_sec - t1.tv_sec ) * 1000000 + (t2.tv_usec - t1.tv_usec);
	msg.pdebug(1, "Ausführungszeit: %d usec", diff);

}

void Http::disconnect(int client)
{
}

void Http::add_provider(HttpProvider *p)
{
	std::string path;
	path = p->getPath();

	if (provider.find(path) == provider.end() )
	{
		msg.pdebug(D_CON, "Provider \"%s\" wird hinzugefügt", path.c_str());
		provider[path] = p;
	}
	else
	{
		msg.perror(E_PRO_EXISTS, "HttpProvider \"%s\" ist schon registriert",
				path.c_str());
	}
}

void Http::del_provider(HttpProvider *p)
{
	std::string path;
	path = p->getPath();
	Provider::iterator i;

	msg.pdebug(D_CON, "Provider \"%s\" wird entfernt", path.c_str());
	if ( (i = provider.find(path)) != provider.end() )
	{
		provider.erase(i);
	}
	else
	{
		msg.perror(E_PRO_NOT_EXISTS,
				"HttpProvider \"%s\" ist nicht registriert", path.c_str());
	}
}


void Http::mk_error(const char *typ, char *str)
{
	int i;
    std::string e;

	if (act_h->client < 0)
		return;

	act_h->status = -200;

	if (act_h->content_type == "text/xml")
	{
		std::string e("<v class=\"");
		e = e + typ + "\">"+ ToString::mkxml(str) + "</v>";
		act_h->error_messages.push_back(e);
	}
	else
	{
		for (i=0; str[i] != '\0'; i++)
		{
			if (str[i] == '\\')
				e.push_back('\\');
			if (str[i] == '\'')
				e.push_back('\\');
			if (str[i] != '\n' && str[i] != '\r')
				e.push_back(str[i]);
			else
			{
				if ( act_h->content_type == "text/html" )
					act_h->error_messages.push_back(std::string("<div class='print_") + typ + "'>" + e + "</div>");
				else
					act_h->error_messages.push_back(e);

				while (str[i] == '\n' || str[i] == '\r')
					i++;
				i--;
				e = "";
			}
		}

		if (e != "")
		{
			if ( act_h->content_type == "text/html" )
				act_h->error_messages.push_back(std::string("<div class='print_") + typ + "'>" + e + "</div>");
			else
				act_h->error_messages.push_back(e);
		}
	}

}

void Http::pwarning(char *str)
{
	mk_error("warning", str);
	act_h->warning_found++;
}

void Http::perror(char *str)
{
	mk_error("error", str);
	act_h->error_found++;
}

void Http::pmessage(char *str)
{
	mk_error("message", str);
	act_h->message_found++;
}

void Http::pline(char *str)
{
	mk_error("line", str);
	act_h->message_found++;
}

