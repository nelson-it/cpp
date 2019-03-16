#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <mbctype.h>
#endif

#include <unistd.h>

#include <utils/cslist.h>
#include <utils/gettimeofday.h>
#include <crypt/base64.h>
#include <crypt/sha1.h>
#include <argument/argument.h>

#include "http.h"
#include "http_analyse.h"

void *HttpAnalyseThread(void *param)
{

#if defined(__MINGW32__) || defined(__CYGWIN__)
	    _setmbcp(_MB_CP_ANSI);
#endif

	HttpAnalyse::HttpAnalyseThreadParam *p;

	p = (HttpAnalyse::HttpAnalyseThreadParam*)param;
	p->http->init_thread();

	while(1)
	{
		pthread_mutex_lock(&p->mutex);
		if ( p->abort ) break;

		p->act_h = p->analyse->getHeader();
		//usleep((rand() % 1000000 ));
		p->http->get(p->act_h);
		p->analyse->releaseHeader(p->act_h);
		p->act_h = NULL;
		pthread_mutex_unlock(&p->mutex);
	}

	pthread_mutex_unlock(&p->mutex);
	pthread_exit(NULL);
	return NULL;
}

HttpAnalyse::HttpAnalyseThreadParam::HttpAnalyseThreadParam(Http *http, HttpAnalyse *analyse )
{
	this->http = http;
	this->analyse = analyse;
	this->abort = 0;
	this->act_h = NULL;

	pthread_mutex_init(&this->mutex,NULL);
	pthread_create(&(this->id), NULL, HttpAnalyseThread, (void *)this);
}

HttpAnalyse::HttpAnalyseThreadParam::~HttpAnalyseThreadParam()
{
	this->abort = 1;
	pthread_mutex_lock(&mutex);
	pthread_mutex_unlock(&mutex);
}

void HttpAnalyse::HttpAnalyseThreadParam::disconnect(int client)
{
    this->http->disconnect(client);
}


HttpHeader *HttpAnalyse::getHeader()
{
	HttpHeader *ret = NULL;

	pthread_mutex_lock(&header_mutex);
	while ( waiting_headers.empty() )
		pthread_cond_wait(&header_cond, &header_mutex);

	ret = waiting_headers.front();
	waiting_headers.pop_front();
	pthread_mutex_unlock(&header_mutex);

	return ret;
}

void HttpAnalyse::putHeader(HttpHeader *h)
{
	pthread_mutex_lock(&header_mutex);
	waiting_headers.push_back(h);
	pthread_cond_signal(&header_cond);
	pthread_mutex_unlock(&header_mutex);
}

void HttpAnalyse::releaseHeader(HttpHeader *h)
{
	delete h;
}

HttpAnalyse::HttpAnalyse(ServerSocket *s)
            :HttpRequest(s),
            SocketProvider(s),
             msg("HttpAnalyse")
{
	this->act_h = NULL;

	pthread_mutex_init(&this->header_mutex,NULL);
	pthread_cond_init(&this->header_cond,NULL);

	s->add_provider(this);
}

HttpAnalyse::~HttpAnalyse()
{
}

void HttpAnalyse::analyse_header()
{
	Headers::iterator h;
	Header::iterator i;

	std::string name;
	std::string arg;
	std::string forward_addr("-"), forward_host("-"), forward_port("-"), forward_proto("-");

	msg.pdebug(D_HTTP, "Analysiere header für client %d", act_h->client );

	if ( ( h = headers.find(act_h->client) ) == headers.end() )
	{
		msg.perror(E_CLIENT_UNKOWN, "Client %d ist unbekannt\n");
		return;
	}

	if ( h->second.size() < 2 )
	{
		msg.perror(E_REQUEST_TO_SHORT, "Zu wenig Zeilen im Request");
		return;
	}

	HttpRequest::analyse_header(&(h->second), act_h);
}

void HttpAnalyse::read_postvalues()
{
	if ( act_h->post_type.find("multipart/form-data") != std::string::npos )
	{
		std::string::size_type npos;

		if ( ( npos = act_h->post_type.find("boundary=")) != std::string::npos )
			act_h->vars.setMultipart( "--" + act_h->post_type.substr(npos + 9), act_h->post_data);
	}
	else
	{
		act_h->vars.setVars(act_h->post_data);
	}

	if ( act_h->post_data != NULL )
	{
		delete[] act_h->post_data;
		act_h->post_data = NULL;
	}
}


void HttpAnalyse::request( int client, char *buffer, long size )
{

	HttpHeaders::iterator chttp;
	Headers::iterator h;
	char *c;
	std::string str;

	act_h = NULL;

    if ( ( h = headers.find(client) ) == headers.end() )
	{
		Header tmp;
		headers[client] = tmp;
		headers[client].push_back("");

		msg.pdebug(D_CON, "neuer Client %d ",client);

	}
	else  if ( ( chttp = http_headers.find(client) ) != http_headers.end() )
	{
		act_h = chttp->second;
	}


	h = headers.find(client);
	str = h->second[0];

	for(c = buffer; ( c - buffer ) < size || ( act_h != NULL && act_h->needed_postdata == 0); c++ )
	{
		if ( act_h != NULL && act_h->client == client && act_h->needed_postdata > 0 )
		{
			int b_rest;

            msg.pdebug(D_CON, "bekomme noch %d Daten",size);
            msg.pdebug(D_CON, "benötige noch %d Daten",  act_h->needed_postdata);

			b_rest = size - ( c - buffer );
			if ( b_rest > act_h->needed_postdata )
				b_rest = act_h->needed_postdata;

            msg.pdebug(D_CON, "lese noch %d Daten", b_rest);

            memcpy(&act_h->post_data[act_h->post_length-act_h->needed_postdata],c, b_rest);
			act_h->needed_postdata -= b_rest;
			c = c + b_rest - 1;

			if ( act_h->needed_postdata == 0 )
				read_postvalues();

		}

		else if ( act_h != NULL && act_h->client == client && act_h->needed_postdata == 0 )
		{
			check_user(act_h);
			putHeader(act_h);
			http_headers.erase(chttp);
			act_h = NULL;

			c--;
		}
		else if ( *c == '\n' )
		{
			if ( str != "" )
			{
				h->second.push_back(str);
				msg.pdebug(D_RAWHEADER, "%s", str.c_str());
				str.clear();
			}
			else
			{
				msg.pdebug(D_SEND, "%s", "start request");

				http_headers[client] = act_h = new HttpHeader;
				chttp = http_headers.find(client);

				act_h->client = client;
				analyse_header();
				act_h->needed_postdata = act_h->post_length;
				act_h->post_data = new char[act_h->post_length + 1];
				act_h->post_data[act_h->post_length] = '\0';
			}
		}
		else if ( *c != '\r' )
		{
			str.push_back(*c);
		}
	}
	h->second[0] = str;
}

void HttpAnalyse::disconnect( int client )
{
	Headers::iterator h;
	unsigned int i;

	if ( ( h = headers.find(client)) != headers.end() )
	{
		msg.pdebug(D_CON, "Verbindung zum Client %d wurde abgebrochen",client);
		headers.erase(h);
	}
	else
	{
		msg.pdebug(D_CON, "Client %d existiert nicht",client);
	}

	for ( i = 0; i<https.size(); ++i)
	    https[i]->disconnect(client);


}

void HttpAnalyse::add_http(Http *http)
{
	https.push_back(new HttpAnalyseThreadParam(http, this));
}

void HttpAnalyse::del_http(Http *http)
{
	msg.pwarning(W_HTTP, "Http kann nicht gelöscht werden");
	return;
}

