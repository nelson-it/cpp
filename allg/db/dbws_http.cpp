#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <sec_api/stdio_s.h>
#endif

#include <argument/argument.h>

#include <unistd.h>

#include "dbws_http.h"

DbWsHttp::DbWsHttp( ServerSocket *s, Ws *ws, DbHttpAnalyse *a, Database *db ) :
	HttpRequest(s),
	DbHttp(s, a, db, 0),
    msg("DbWsHttp")
{
	if ( ws != NULL )
		ws->add_provider(this);
}

DbWsHttp::~DbWsHttp()
{
}

void DbWsHttp::analyse_header(const unsigned char *data, int length)
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

void DbWsHttp::make_header()
{
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%d\n%s\n", act_h.status, act_h.header["telnum"].c_str());
    buffer[sizeof(buffer) - 1] = '\0';
    header = buffer;
}

int DbWsHttp::request(WsAnalyse::Client *c)
{
    act_h.clear();
    analyse_header(c->p_getData(), c->getLength());


    msg.add_msgclient(this);
    setThreadonly();

    make_content(&act_h);
    make_header();

    msg.del_msgclient(this);

    return 1;
}
