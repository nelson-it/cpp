#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <sec_api/stdio_s.h>
#endif

#include <argument/argument.h>

#include <unistd.h>

#include "ws_http.h"
#include "ws_provider.h"

WsHttp::WsHttp( ServerSocket *s, Ws *ws) :
	HttpRequest(s),
	Http(s, NULL),
    msg("WsHttp")
{
	if ( ws != NULL )
		ws->add_provider(this);
}

WsHttp::~WsHttp()
{
}

void WsHttp::read_header(const unsigned char *data, int length)
{
    const unsigned char *c = data;
    const unsigned char *start = data;
    const unsigned char *end = data + length;
    std::string str;
    Header h;

    // Lines
    // =====
    while ( c != end )
    {
        for ( start = c; c != end && *c != '\n' && *c != '\r'; ++c );
        str.assign((char *)start, c - start);
        h.push_back(str);
        while ( c != end && ( *c == '\n' || *c == '\r') ) ++c;
    }

    HttpRequest::analyse_header(&h, &act_h);
}

void WsHttp::make_header()
{
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%d\n%s\n", act_h.status, act_h.header["telnum"].c_str());
    buffer[sizeof(buffer) - 1] = '\0';
    header = buffer;
}



int WsHttp::request(WsAnalyse::Client *c)
{
    read_header(c->p_getData(), c->getLength());
    this->make_content(&act_h);

    return 1;
}
