#include <pthread.h>
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

Http::Http(ServerSocket *s, HttpAnalyse *analyse ) :
	msg("Http")
{

	Argument a;
	std::string mstr;
	CsList mlist;
	CsList mele;

	no_cache = a["EmbedwebHttpNocache"];

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
	          = "<!DOCTYPE html>"
	                  "<html lang=\"de-DE\">"
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
	          = "<!DOCTYPE html>"
	                  "<html lang=\"de-DE\">"
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
	          = "<!DOCTYPE html>"
	                  "<html lang=\"de-DE\">"
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
	          = "<!DOCTYPE html>"
	                  "<html lang=\"de-DE\">"
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
	this->act_h = NULL;

	if (analyse != NULL )
		analyse->add_http(this);
}

Http::~Http()
{
    if ( analyse != NULL )
	    analyse->del_http(this);
}

void Http::make_meldung()
{
    FILE *fp;
    char buffer[102400];
    std::string str;
	unsigned int i;

    if (act_h->filename.find(".xml") == (act_h->filename.size() - 4))
    {
        act_h->content_type = "text/xml";
        add_content(act_h,  "<?xml version=\"1.0\" encoding=\"%s\"?><result>", act_h->charset.c_str());
        add_content(act_h,  "<body>error</body>");
        return;
    }

    act_h->error_messages.clear();
    act_h->error_types.clear();

    sprintf(buffer, "%d", act_h->status);

    act_h->content_type = "text/html";
    for (i=0; i<act_h->serverpath.size(); i++)
    {
    	struct stat s;
        str = act_h->serverpath[i] + "/state/" + buffer + ".html";
        if (stat(str.c_str(), &s) == 0 ) break;
    }

    if ( (fp = fopen(str.c_str(), "rb") ) == NULL )
    {
        if ( meldungen.find(act_h->status) != meldungen.end())
        {
           str = meldungen[act_h->status].c_str();
        }
        else
        {
            act_h->content_type = "text/plain";
            sprintf(buffer, "Status: %d", act_h->status);
            str = buffer;
        }
    }
    else
    {
        int num = fread(buffer, 1, sizeof(buffer) - 1, fp);
        buffer[num] = '\0';
        str = buffer;
    }

    std::string::size_type pos = str.find("#request#");
    if (pos != std::string::npos)
        str.replace(pos, 9, act_h->dirname + "/" + act_h->filename);

    add_content(act_h, str);
    act_h->translate = 1;
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
		make_meldung();
		return;
	}

	if (act_h->status != 404 )
	{
		act_h->age = 0;
		make_meldung();
		return;
	}

	if ( (p = find_provider(&provider)) != NULL)
	{
		if ( !p->request(act_h) )
		{
			act_h->status = 404;
			act_h->age = 0;
			msg.perror(E_NOTFOUND, "Provider %s unterstützt %s/%s nicht", act_h->providerpath.c_str(), act_h->dirname.c_str(), act_h->filename.c_str());
		    make_meldung();
		}
		return;
	}

	for (i=0; i<act_h->serverpath.size(); i++)
	{
		str = act_h->serverpath[i] + "/" + act_h->dirname + "/" + act_h->filename;
		if (stat(str.c_str(), &s) == 0 ) break;
	}

	if ( (file = fopen(str.c_str(), "rb") ) == NULL)
	{
		act_h->status = 404;
		act_h->age = 0;
	    if ( act_h->vars["ignore_notfound"] == "" )
            msg.perror(E_NOTFOUND, "Kann Datei %s/%s nicht finden", act_h->dirname.c_str(), act_h->filename.c_str());

		 make_meldung();
	}
	else
	{
		act_h->status = 200;

		act_h->translate = 1;
		if ( act_h->setstatus  )
		    act_h->status = act_h->setstatus;

        if (act_h->filename.find(".php") == (act_h->filename.size() - 4 ))
        {
            act_h->age = 0;
            PhpExec(str, act_h);
        }
		else
		{
     		act_h->age = 3600;
		    contentf(act_h, file);
		}

		fclose(file);
	}
}

void Http::make_translate()
{
    if ( act_h->content_type.find("text") == 0 && act_h->translate )
    {
        http_translate.make_answer(act_h, NULL);
        act_h->translate = 0;
    }
}

void Http::make_error()
{
    if ( !act_h->error_messages.empty() )
    {
        unsigned int i,j;

        if (act_h->content_type == "text/xml")
        {
            add_content(act_h,  "<error>\n");
            for (i = 0; i< act_h->error_messages.size(); ++i)
                add_content(act_h,  "<v class=\"%s\">%s</v>\n", act_h->error_types[i].c_str(), ToString::mkxml(act_h->error_messages[i]).c_str());
            add_content(act_h,  "\n</error>");
        }
        else if ( act_h->content_type == "text/json" )
        {
            std::string komma;
            add_content(act_h,  ",\"meldungen\": [\n");
            for (i = 0; i< act_h->error_messages.size(); ++i)
            {
                add_content(act_h,  (komma + "  [ \"%s\", \"%s\" ]\n" ).c_str(), act_h->error_types[i].c_str(), ToString::mkjson(act_h->error_messages[i]).c_str());
                komma = ",";
            }
            add_content(act_h,  "]");
        }
        else
        {
            for (i = 0; i< act_h->error_messages.size(); ++i)
            {
                std::string str = act_h->error_messages[i];
                std::string e;

                for (j=0; str[j] != '\0'; j++)
                {
                    if (str[j] == '\\')
                        e.push_back('\\');
                    if (str[j] == '\'')
                        e.push_back('\\');
                    if (str[j] != '\n' && str[j] != '\r')
                        e.push_back(str[j]);
                    else
                    {
                        if ( act_h->content_type == "text/html" )
                            add_content(act_h,  "<div class='print_%s'>%s</div>\n", act_h->error_types[i].c_str(), e.c_str());
                        else
                            add_content(act_h,  "%s: %s\n", act_h->error_types[i].c_str(), e.c_str());;

                        while (str[j] == '\n' || str[j] == '\r')
                            j++;
                        j--;
                        e = "";
                    }
                }

                if (e != "")
                {
                    if ( act_h->content_type == "text/html" )
                        add_content(act_h,  "<div class='print_%s'>%s</div>\n", act_h->error_types[i].c_str(), e.c_str());
                    else
                        add_content(act_h,  "%s: %s\n", act_h->error_types[i].c_str(), e.c_str());;
                }
            }

        }
    }

    if ( act_h->content_type == "text/xml" )
        add_content(act_h, "</result>");
    else if ( act_h->content_type == "text/json" )
        add_content(act_h, "}");

}

void Http::write_header()
{
	HttpHeader::SetCookies::iterator i;
    int status;
    unsigned int count;
	char buffer[10240];

	status = act_h->status;
	if ( !act_h->error_messages.empty() )
	{
		HttpVars::Vars::iterator v;
		msg.perror(E_WRITEHEADER, "Headerdaten:");
		msg.line("%s: %s", msg.get("Type").c_str(), act_h->typ.c_str());
		msg.line("%s: %s", msg.get("Dirname").c_str(), act_h->dirname.c_str());
		msg.line("%s: %s", msg.get("Filename").c_str(), act_h->filename.c_str());
		msg.line("%s: %s", msg.get("Protokoll").c_str(), act_h->protokoll.c_str());
		msg.line("%s: %s", msg.get("Hostname").c_str(), act_h->hostname.c_str());
		msg.line("%s: %s", msg.get("User").c_str(), act_h->user.c_str());

		v = act_h->vars.p_getVars()->begin();
		for (; v != act_h->vars.p_getVars()->end(); ++v)
			msg.line("%s: %s", v->first.c_str(), v->second.c_str());

	}
	msg.pdebug(D_HTTP, "schreibe header - status \"%d\",\"%s\" \"%s\" %d", status, status_str[status].c_str(), act_h->content_type.c_str(), act_h->content_length);

	sprintf(buffer, "HTTP/1.1 %d %s\r\n", status, status_str[status].c_str());
	s->write(act_h->client, buffer, strlen(buffer));

    for ( count = 0; count < act_h->extra_header.size(); ++count )
    {
        sprintf(buffer, "%s\r\n", act_h->extra_header[count].c_str());
        s->write(act_h->client, buffer, strlen(buffer));
    }

    if ( status == 101 )
    {
        sprintf(buffer, "\r\n");
        s->write(act_h->client, buffer, strlen(buffer));
        act_h->content_length = 0;
        return;
    }

	if ( status == 401 )
	{
		msg.pdebug(D_HTTP, "fordere Passwort an");
		if ( act_h->realm.empty() )
			sprintf(buffer, "WWW-Authenticate: Basic realm=\"%d\"\r\n", ((int)time(0)));
		else
			sprintf(buffer, "WWW-Authenticate: Basic realm=\"%s\"\r\n", act_h->realm.c_str());
		s->write(act_h->client, buffer, strlen(buffer));
		buffer[strlen(buffer) - 2 ] = '\0';
		msg.pdebug(D_CON, "fordere Passwort an %s", buffer);
	}

	sprintf(buffer, "Server: M Nelson Embedded Http Server 0.9\r\n");
	s->write(act_h->client, buffer, strlen(buffer));

	if ( no_cache || act_h->age == 0 )
	{
		msg.pdebug(D_HTTP, "Cache_Control: no-store");
		sprintf(buffer, "Cache-Control: no-store\r\n");
	}

	s->write(act_h->client, buffer, strlen(buffer));

	if (act_h->connection)
	{
		sprintf(buffer, "Connection: Keep-Alive\r\n");
		s->write(act_h->client, buffer, strlen(buffer));
	}
	else
	{
		sprintf(buffer, "Connection: Close\r\n");
		s->write(act_h->client, buffer, strlen(buffer));

	}

	for (i = act_h->set_cookies.begin(); i != act_h->set_cookies.end(); ++i)
	{
		sprintf(buffer, "Set-Cookie: %s=%s; path=/; sameSite=Strict\r\n", i->first.c_str(), i->second.c_str());
		s->write(act_h->client, buffer, strlen(buffer));
	}

    sprintf(buffer, "Accpet-Ranges: bytes\r\n");
    s->write(act_h->client, buffer, strlen(buffer));

	sprintf(buffer, "Content-Length: %ld\r\n", act_h->content_length);
	s->write(act_h->client, buffer, strlen(buffer));

	sprintf(buffer, "Content-Type: %s; charset=%s\r\n", act_h->content_type.c_str(), act_h->charset.c_str());
	s->write(act_h->client, buffer, strlen(buffer));

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
	msg.pdebug(D_HTTP, "write trailer - status \"%d\",\"%s\"", act_h->connection, act_h->connection ? "behalten" : "schliessen");

	if ( !act_h->connection)
		s->close(act_h->client);
}

void Http::make_content(HttpHeader *h)
{
    if ( h != NULL ) act_h = h;
    if ( act_h == NULL ) return;

    make_answer();
    make_error();
    make_translate();
}

void Http::get(HttpHeader *h)
{
	struct timeval t1, t2;
	long diff;

	gettimeofday(&t1, NULL);

	act_h = h;
    h->content[0] = '\0';
    h->content_length = 0;

	msg.add_msgclient(this);
	setThreadonly();

    make_content();

    write_header();
    write_content();
    write_trailer();

    s->flush(act_h->client);

	msg.del_msgclient(this);
	act_h = NULL;

	gettimeofday(&t2, NULL);
	diff = (t2.tv_sec - t1.tv_sec ) * 1000000 + (t2.tv_usec - t1.tv_usec);
	msg.pdebug(1, "Ausführungszeit: %d usec", diff);

}

void Http::disconnect(int client)
{
    Provider::iterator i;

    for ( i = provider.begin(); i != provider.end(); ++i)
        i->second->disconnect(client);

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
		msg.perror(E_PRO_EXISTS, "HttpProvider \"%s\" ist schon registriert", path.c_str());
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

        msg.pdebug(D_HTTP, "Provider: %s dirname: %s filename: %s", act_p->first.c_str(), act_h->dirname.c_str(), act_h->filename.c_str());
        return act_p->second;
    }
    else
    {
        return NULL;
    }
}

void Http::mk_error(const char *typ, char *str)
{
	act_h->error_messages.push_back(str);
	act_h->error_types.push_back(typ);

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

#if defined(__MINGW32__) || defined(__CYGWIN__)
int inet_aton(const char *cp_arg, struct in_addr *addr)
{
    register const u_char *cp = (u_char*)cp_arg;
    register u_long val;
    register int base;
#ifdef WIN32
    register ULONG_PTR n;
#else
    register unsigned long n;
#endif
    register u_char c;
    u_int parts[4];
    register u_int *pp = parts;

    for (;;) {
        /*
         * Collect number up to ``.''.
         * Values are specified as for C:
         * 0x=hex, 0=octal, other=decimal.
         */
        val = 0; base = 10;
        if (*cp == '0') {
            if (*++cp == 'x' || *cp == 'X')
                base = 16, cp++;
            else
                base = 8;
        }
        while ((c = *cp) != '\0') {
            if (isascii(c) && isdigit(c)) {
                val = (val * base) + (c - '0');
                cp++;
                continue;
            }
            if (base == 16 && isascii(c) && isxdigit(c)) {
                val = (val << 4) +
                        (c + 10 - (islower(c) ? 'a' : 'A'));
                cp++;
                continue;
            }
            break;
        }
        if (*cp == '.') {
            /*
             * Internet format:
             * a.b.c.d
             * a.b.c (with c treated as 16-bits)
             * a.b (with b treated as 24 bits)
             */
            if (pp >= parts + 3 || val > 0xff)
                return (0);
            *pp++ = val, cp++;
        } else
            break;
    }
    /*
     * Check for trailing characters.
     */
    if (*cp && (!isascii(*cp) || !isspace(*cp)))
        return (0);
    /*
     * Concoct the address according to
     * the number of parts specified.
     */
    n = pp - parts + 1;
    switch (n) {

    case 1: /* a -- 32 bits */
        break;

    case 2: /* a.b -- 8.24 bits */
        if (val > 0xffffff)
            return (0);
        val |= parts[0] << 24;
        break;

    case 3: /* a.b.c -- 8.8.16 bits */
        if (val > 0xffff)
            return (0);
        val |= (parts[0] << 24) | (parts[1] << 16);
        break;

    case 4: /* a.b.c.d -- 8.8.8.8 bits */
        if (val > 0xff)
            return (0);
        val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
        break;
    }
    if (addr)
        addr->s_addr = htonl(val);
    return (1);
}
#endif

int Http::check_ip(const char *ip)
{
    unsigned long mask = -1;
    CsList ipaddr (ip, '/');
    sockaddr_storage *checkaddr = this->s->getAddr(act_h->client);

    if ( ipaddr.size() == 0 ) return 1;

    if ( checkaddr->ss_family == AF_INET )
    {
        CsList addr(ipaddr[0],'.');
        struct in_addr taddr, caddr;

        if ( addr.size() != 4 )
            return 0;

        if ( ipaddr.size() == 2 )
            mask = htonl(mask << ( 32 - atoi(ipaddr[1].c_str()) ));

        caddr.s_addr = ((struct sockaddr_in *)checkaddr)->sin_addr.s_addr;
        caddr.s_addr &= mask;

        inet_aton(ipaddr[0].c_str(), &taddr );
        taddr.s_addr &= mask;

        if ( caddr.s_addr == taddr.s_addr )
            return 1;
    }
    else if ( checkaddr->ss_family == AF_INET6 )
    {
        struct in6_addr netmask;
        struct in6_addr taddr, caddr;
        long i,j;

        if ( ipaddr[0].find('.') != std::string::npos )
            inet_pton(AF_INET6, ("::ffff:" + ipaddr[0]).c_str(), &taddr);
        else
            inet_pton(AF_INET6, ipaddr[0].c_str(), &taddr);

        memcpy(caddr.s6_addr, ((struct sockaddr_in6 *)checkaddr)->sin6_addr.s6_addr, sizeof(caddr.s6_addr));


        if ( ipaddr.size() == 2 )
        {
            memset(netmask.s6_addr, 0, sizeof(netmask.s6_addr));
            for ( i = atoi(ipaddr[1].c_str()), j = 0; i > 0; i -= 8, ++j)
                netmask.s6_addr[ j ] = i >= 8 ? 0xff : (unsigned long)(( 0xffU << ( 8 - i ) ) & 0xffU );
            for ( i = 0; i < 16; i++)
                caddr.s6_addr[i] = caddr.s6_addr[i] & netmask.s6_addr[i];
        }

        for ( i =0; i< 16; i++)
            if ( taddr.s6_addr[i] != caddr.s6_addr[i] )
                return 0;

        return 1;
    }
    return 0;
}



