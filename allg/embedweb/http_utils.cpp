#include <pthread.h>
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
#include <unistd.h>
#endif

#ifdef MACOS
#include <locale.h>
#endif

#include <argument/argument.h>
#include <utils/tostring.h>

#include "http_utils.h"

HttpUtils::HttpUtils(Http *h) :
	HttpProvider(h), msg("HttpUtils")
{

        subprovider["time.html"] = &HttpUtils::time;
        subprovider["file.html"] = &HttpUtils::file;
        subprovider["file.txt"] = &HttpUtils::file;

        subprovider["locale.xml"] = &HttpUtils::locale_xml;
        subprovider["locale.json"] = &HttpUtils::locale_json;

        subprovider["logout.xml"] = &HttpUtils::logout_xml;
        subprovider["logout.json"] = &HttpUtils::logout_json;

        subprovider["iconv.txt"] = &HttpUtils::iconv;

		if ( h != NULL )
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

void HttpUtils::time(HttpHeader *h)
{
	h->content_type = "text/plain";
	h->status = 200;
	add_content(h,  "%d", ((int)::time(NULL)));
}

void HttpUtils::logout_xml(HttpHeader *h)
{
    Argument a;
    char str[1024];

    h->status = 201;
    h->content_type = "text/xml";

    snprintf(str, sizeof(str), "MneHttpSessionId%d", (int)a["port"]);
    str[sizeof(str) - 1] = '\0';
    h->set_cookies[str] = "Logout";
    add_content(h, "<?xml version=\"1.0\" encoding=\"%s\"?><result><body>logout</body>",h->charset.c_str());
}

void HttpUtils::logout_json(HttpHeader *h)
{
    Argument a;
    char str[1024];

    h->status = 201;
    h->content_type = "text/json";

    snprintf(str, sizeof(str), "MneHttpSessionId%d", (int)a["port"]);
    str[sizeof(str) - 1] = '\0';
    h->set_cookies[str] = "Logout";
    add_content(h, "{ \"result\" : \"logout\" ");
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

        if ( ( iv = iconv_open("utf-8//TRANSLIT", h->vars["iconv"].c_str()) ) != (iconv_t)(-1))
        {
            ::iconv (iv, &ci, &innum, &co, &outnum);
            iconv_close(iv);

            *co = '\0';
            str = outbuf;
            delete[] outbuf;
        }
    }

    if ( h->vars["script"] != "" )
    {
        add_content(h, "<script type=\"text/javascript\">\n");
        add_content(h, "<!--\n");
        add_content(h, "%s\n", h->vars["script"].c_str());
        add_content(h, "//-->\n");
        add_content(h, "</script>\n");
        if ( h->content_type == "text/plain")
        {
            h->content_type = "text/html";
            if ( h->vars["data"].substr(0,10) == "##########" )
                add_content(h,  "<textarea id=\"data\" >%s</textarea>", str.c_str());
            else
                add_content(h,  "<textarea id=\"data\" >%s</textarea>", ToString::mkhtml(str.c_str()).c_str());
            return;
        }
    }
    add_content(h, "%s%s",h->vars["data"].c_str(), endtag.c_str());
}

void HttpUtils::iconv(HttpHeader *h)
{
    std::string endtag;
    iconv_t iv;
    char *inbuf, *outbuf, *ci, *co;
    size_t innum,outnum;
    std::string str;

    h->status = 200;
    h->content_type = "text/plain";

    str = h->vars["data"];

    if ( h->vars["iconv"] != "" )
    {
        ci = inbuf = (char *)str.c_str();
        innum = str.length();

        co = outbuf = new char[str.size() * 4];
        outnum = ( str.size() * 4 - 1);

        if ( ( iv = iconv_open("utf-8//TRANSLIT", h->vars["iconv"].c_str()) ) != (iconv_t)(-1))
        {
            ::iconv (iv, &ci, &innum, &co, &outnum);
            iconv_close(iv);

            *co = '\0';
            str = outbuf;
            delete[] outbuf;
        }
    }

    add_content(h, str);
}

void HttpUtils::locale_xml(HttpHeader *h)
{
    h->status = 200;
    h->content_type = "text/xml";

    struct lconv *l;
    l = localeconv();

    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1 ] = '\0';

    add_content(h, "<?xml version=\"1.0\" encoding=\"%s\"?><result><head><d><id>decimal_point</id><typ>2</typ><name>decimal_point</name></d><d><id>thousands_sep</id><typ>2</typ><name>thousands_sep</name></d><d><id>hostname</id><typ>2</typ><name>hostname</name></d></head><body>",h->charset.c_str());
#if defined(__MINGW32__) || defined(__CYGWIN__)
    add_content(h,  "<r><v>%s</v><v>%s</v><v>%s</v></r></body>", l->decimal_point, "", hostname );
#else
    add_content(h,  "<r><v>%s</v><v>%s</v><v>%s</v></r></body>", l->decimal_point, l->thousands_sep, hostname );
#endif
    return;
}
void HttpUtils::locale_json(HttpHeader *h)
{
    h->status = 200;
    h->content_type = "text/json";

    struct lconv *l;
    l = localeconv();

    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1 ] = '\0';

    add_content(h, "{ \"ids\" : [ \"decimal_point\", \"thousands_sep\", \"hostname\" ],\n \"labels\" : [ \"decimal_point\", \"thousands_sep\", \"hostname\" ],\n \"values\" : [[ ");
#if defined(__MINGW32__) || defined(__CYGWIN__)
    add_content(h,  "\"%s\", \"%s\", \"%s\" ]] ", l->decimal_point, "", hostname );
#else
    add_content(h,  "\"%s\", \"%s\", \"%s\" ]] ", l->decimal_point, l->thousands_sep, hostname );
#endif
    return;
}
