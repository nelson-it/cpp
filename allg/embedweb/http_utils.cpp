#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iconv.h>
#include <errno.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include <argument/argument.h>
#include <utils/tostring.h>

#include "http_utils.h"

HttpUtils::HttpUtils(Http *h) :
	HttpProvider(h), msg("HttpUtils")
{

		subprovider["count.html"] = &HttpUtils::count;
		subprovider["uhr.html"] = &HttpUtils::uhr;
        subprovider["time.html"] = &HttpUtils::time;
        subprovider["logout.xml"] = &HttpUtils::logout;
        subprovider["file.html"] = &HttpUtils::file;
        subprovider["file.txt"] = &HttpUtils::file;
        subprovider["locale.xml"] = &HttpUtils::locale;
        subprovider["proxy.txt"] = &HttpUtils::proxy;

		h->add_provider(this);
}

HttpUtils::~HttpUtils()
{
}

int HttpUtils::request(HttpHeader *h)
{

	SubProviderMap::iterator i;

	if ( (i = subprovider.find(h->filename)) != subprovider.end() )
	{
		(this->*(i->second))(h);
		return 1;
	}

	return 0;

}

void HttpUtils::mk_window(HttpHeader *h, char *str)
{

	h->status = 200;
	h->content_type = "text/html";

	fprintf(h->content, "<!DOCTYPE HTML PUBLIC \"-");
	fprintf(h->content, "//W3C//DTD HTML 4.01 Transitional//DE\">");
	fprintf(h->content, "<html>");
	fprintf(h->content, "<head>");
	fprintf(h->content, "<title>Nelson technische Informatik - count</title>");
	fprintf(h->content, "<meta http-equiv=\"Content-Type\" ");
	fprintf(h->content, "content=\"text/html; ");
	fprintf(h->content, "charset=iso-8859-1\">");
	fprintf(h->content, "<link id=\"MainStyleLink\" rel=\"stylesheet\" ");
	fprintf(h->content, "type=\"text/css\">");
	fprintf(h->content, "<script language=\"JavaScript\" ");
	fprintf(h->content, "type=\"text/JavaScript\">");
	fprintf(h->content, "window.onload = function() ");
	fprintf(h->content, "{ ");
	fprintf(h->content, "top.mneMain.mneMisc.setStyle(document, ");
	fprintf(h->content, "\"MainStyleLink\");");
	fprintf(h->content, "var d = new Date(); ");
	fprintf(h->content, "document.getElementById('uhr').firstChild.data = ");
	fprintf(h->content, " top.mneMain.mneMisc.addNull(d.getHours(),2) + ':' ");
	fprintf(h->content, " + top.mneMain.mneMisc.addNull(d.getMinutes(),2); } ");
	fprintf(h->content, "</script>\n");
	fprintf(h->content, "</head>\r\n");
	fprintf(h->content, "<body class=\"MneCount\">\r\n");
	fprintf(h->content, "<div id=\"uhr\" class=\"MneCount\">%s</div>\r\n", str);
	fprintf(h->content, "</body>");
}

void HttpUtils::count(HttpHeader *h)
{
	mk_window(h, (char *)h->vars["count"].c_str());
}

void HttpUtils::uhr(HttpHeader *h)
{
	char buffer[32];
	time_t t;

	::time(&t);
	strftime(buffer, sizeof(buffer), "%H:%M", localtime(&t));
	mk_window(h, buffer);
}

void HttpUtils::time(HttpHeader *h)
{
	h->content_type = "text/plain";
	h->status = 200;
	fprintf(h->content, "%d", ((int)::time(NULL)));
}

void HttpUtils::logout(HttpHeader *h)
{
    Argument a;
    char str[1024];

    h->status = 200;
    h->content_type = "text/xml";

    snprintf(str, sizeof(str), "MneHttpSessionId%d", (int)a["port"]);
    str[sizeof(str) - 1] = '\0';
    h->set_cookies[str] = "Logout";
    fprintf(h->content,"<?xml version=\"1.0\" encoding=\"%s\"?><result><body>logout</body>",h->charset.c_str());
}

void HttpUtils::file(HttpHeader *h)
{
    std::string endtag;
    iconv_t iv;
    char *inbuf, *outbuf, *ci, *co;
    size_t innum,outnum;
    std::string str;

    h->status = 200;
    if ( h->filename == "file.html")
    {
        h->content_type = "text/html";
    }
    else
    {
        h->content_type = "text/plain";
    }

    str = h->vars["data"];
    if ( str.substr(0,10) == "##########" )
        str = h->vars.data("data", 1).c_str();

    if ( h->vars["iconv"] != "" )
    {
        ci = inbuf = (char *)str.c_str();
        innum = str.length();

        co = outbuf = new char[str.size() * 4];
        outnum = ( str.size() * 4 - 1);

        if ( ( iv = iconv_open("utf-8//TRANSLIT", h->vars["iconv"].c_str()) ) >= 0 )
        {
            iconv (iv, &ci, &innum, &co, &outnum);
            iconv_close(iv);

            *co = '\0';
            str = outbuf;
            delete[] outbuf;
        }
    }

    if ( h->vars["script"] != "" )
    {
        fprintf(h->content,"<script type=\"text/javascript\">\n");
        fprintf(h->content,"<!--\n");
        fprintf(h->content,"%s\n", h->vars["script"].c_str());
        fprintf(h->content,"//-->\n");
        fprintf(h->content,"</script>\n");
        if ( h->content_type == "text/plain")
        {
            h->content_type = "text/html";
            if ( h->vars["data"].substr(0,10) == "##########" )
                fprintf(h->content, "<textarea id=\"data\" >%s</textarea>", str.c_str());
            else
                fprintf(h->content, "<textarea id=\"data\" >%s</textarea>", ToString::mkhtml(str.c_str()).c_str());
            return;
        }
    }
    fprintf(h->content,"%s%s",h->vars["data"].c_str(), endtag.c_str());
}

void HttpUtils::locale(HttpHeader *h)
{
    h->status = 200;
    h->content_type = "text/xml";

    struct lconv *l;
    l = localeconv();

    fprintf(h->content,"<?xml version=\"1.0\" encoding=\"%s\"?><result><head><d><id>decimal_point</id><typ>2</typ><name>decimal_point</name></d><d><id>thousands_sep</id><typ>2</typ><name>thousands_sep</name></d></head><body>",h->charset.c_str());
    fprintf(h->content, "<r><v>%s</v><v>%s</v></r></body>",l->decimal_point, l->thousands_sep);

    return;
}

void HttpUtils::proxy(HttpHeader *h)
{
    char tmp[10240];
#if defined(__MINGW32__) || defined(__CYGWIN__) || defined(Darwin)
    struct hostent *hp;
#else
    struct hostent hostbuf, *hp;
    int herr;
#endif
    struct sockaddr_in s_in;
    int port = 80;
    int sock;
    std::string request;

    h->content_type = "text/plain";

    if (h->vars["port"] != "" ) port = atoi(h->vars["port"].c_str());

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        msg.perror(E_OPEN, "Konnte Socket nicht öffnen");
        return;
    }

#if defined(__MINGW32__) || defined(__CYGWIN__) || defined(Darwin)
    hp = gethostbyname(h->vars["host"].c_str());
#else
    gethostbyname_r(h->vars["host"].c_str(), &hostbuf, tmp, sizeof(tmp), &hp, &herr);
#endif
    if (hp == NULL)
    {
        msg.perror(E_HOST, "Unbekannter Server <%s>", h->vars["host"].c_str());
        close( sock);
        sock = -1;
        return;
    }

    memset((char*) &s_in, 0, sizeof(s_in));
    memcpy(&s_in.sin_addr, hp->h_addr, hp->h_length);
    s_in.sin_family = AF_INET;
    s_in.sin_port = htons(port);

    if (::connect(sock, (sockaddr*) &s_in, sizeof(struct sockaddr_in)) < 0)
    {
        msg.perror(E_CONNECT, "kann mich nicht mit server <%s:%d> verbinden", h->vars["host"].c_str(), port);
        close( sock);
        sock = -1;
        return;
    }

    h->status = 200;

    request  = "GET " + h->vars["get"] + " HTTP/1.1\r\n";
    request += "Host: " + h->vars["host"] + "\r\n";
    request += "User-Agent: " + h->user_agent + "\r\n";
    request += "Accept: */*\r\n";
    request += "\r\n";

    send(sock, request.c_str(), request.length(), 0);

    int l;
    while( ( l = recv(sock, tmp, sizeof(tmp), 0)) > 0 )
        fwrite(tmp, l, 1, h->content);

    close(sock);
    return;
}
