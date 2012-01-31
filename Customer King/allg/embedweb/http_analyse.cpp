#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <utils/cslist.h>
#include <utils/gettimeofday.h>
#include <crypt/base64.h>
#include <argument/argument.h>

#include "http.h"
#include "http_analyse.h"

#ifdef PTHREAD

void *HttpAnalyseThread(void *param)
{
	Message msg("HttpAnalyseThread", 1);

	HttpAnalyse::HttpAnalyseThreadParam *p;

	p = (HttpAnalyse::HttpAnalyseThreadParam*)param;
	p->http->init_thread();

	while(1)
	{
		pthread_mutex_lock(&p->mutex);
		if ( p->abort ) break;

		p->act_h = p->analyse->getHeader();
		p->http->get(p->act_h);
		p->analyse->releaseHeader(p->act_h);
		p->act_h = NULL;
		pthread_mutex_unlock(&p->mutex);
	}

	pthread_mutex_unlock(&p->mutex);
	pthread_exit(NULL);
	return NULL;
}

HttpAnalyse::HttpAnalyseThreadParam::HttpAnalyseThreadParam(Http *http)
{
	this->http = http;
	this->analyse = http->p_getHttpAnalyse();
	this->abort = 0;

	pthread_mutex_init(&this->mutex,NULL);
	pthread_create(&(this->id), NULL, HttpAnalyseThread, (void *)this);
}

HttpAnalyse::HttpAnalyseThreadParam::~HttpAnalyseThreadParam()
{
	this->abort = 1;
	pthread_mutex_lock(&mutex);
	pthread_mutex_unlock(&mutex);
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

#endif

HttpAnalyse::HttpAnalyse(ServerSocket *s)
:SocketProvider(s),
msg("HttpAnalyse")
{

	Argument a;
	CsList spath;
    unsigned int i;

	spath.setString((char *)a["EmbedwebHttpServerpath"], ':');
	for (i=0; i<spath.size(); i++)
	{
		serverpath.push_back(spath[i]);
	}


	content_types["gif"]  = "image/gif";
	content_types["png"]  = "image/png";
	content_types["jpg"]  = "image/jpeg";
	content_types["jpe"]  = "image/jpeg";
    content_types["jpeg"] = "image/jpeg";
    content_types["ico"]  = "image/x-icon";

	content_types["txt"]  = "text/plain";
	content_types["html"] = "text/html";
	content_types["xml"]  = "text/xml";
	content_types["css"]  = "text/css";
	content_types["js"]   = "text/javascript";

	content_types["gz"]   = "application/gzip";
	content_types["gtar"] = "application/x-gtar";
	content_types["z"]    = "application/x-compress";

	content_types["pdf"]  = "application/pdf";
	content_types["ai"]   = "application/postscript";
	content_types["eps"]  = "application/postscript";
	content_types["ps"]   = "application/postscript";

	this->act_h = NULL;

#ifdef PTHREAD
	pthread_mutex_init(&this->header_mutex,NULL);
	pthread_cond_init(&this->header_cond,NULL);

#endif
	s->add_provider(this);
}

HttpAnalyse::~HttpAnalyse()
{
}

void HttpAnalyse::analyse_requestline( std::string in, HttpHeader *h )
{
	std::string arg;
	std::string::size_type n;

	n = in.find_first_of(' ');
	if ( n != std::string::npos )
	{
		h->typ = in.substr(0, n);
		arg = in.substr(n+1);
	}
	else
	{
		msg.perror(E_REQUEST, "Anforderungszeile %s ist im falschen Format",
				in.c_str());
		return;
	}

	h->serverpath = serverpath;

	n = arg.find_first_of(' ');
	if ( n != std::string::npos )
	{
		std::string request = arg.substr(0, n);
		h->protokoll = arg.substr(n+1);

		if (( n = request.find_first_of('?')) != std::string::npos )
		{
			h->vars.setVars(request.substr(n+1));
			request = request.substr(0, n );
		}

		if ( ( n = request.find_last_of('/') ) != std::string::npos )
		{
			if ( n == 0 )
			{
				h->dirname = "";
				h->filename = request.substr(1);
			}
			else
			{
				h->dirname  = request.substr(0, n );
				h->filename = request.substr(n + 1);
			}

			if ( h->filename == "" )
				h->filename = "index.html";
		}
		else
		{
			h->dirname = "";
			h->filename = request;
		}

		if ( h->filename.find_last_of('.') == std::string::npos )
		{
			h->dirname = h->dirname + "/" + h->filename;
			h->filename = "index.html";
		}

	}
	else
	{
		msg.perror(E_REQUEST, "Anforderungszeile %s ist im falschen Format",
				in.c_str());
		return;
	}
}

void HttpAnalyse::analyse_header()
{
	Headers::iterator h;
	Header::iterator i;

	std::string name;
	std::string arg;
	std::string::size_type n,k;

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

	analyse_requestline( h->second[1], act_h );

	if ( ( n = act_h->filename.find_last_of('.')) != std::string::npos )
	{
		ContentTypes::iterator c;

		if ( ( c = content_types.find(act_h->filename.substr(n+1)) )
				!= content_types.end() )
			act_h->content_type = c->second;
	}


	msg.pdebug(1, "ordner: \"%s\" datei: \"%s\", protokoll \"%s\"",
			act_h->dirname.c_str(),
			act_h->filename.c_str(),
			act_h->protokoll.c_str());

	// Rest des Headers auswerten
	// ==========================

	for( i = h->second.begin() + 2; i != h->second.end(); ++i )
	{
		if ( ( n = i->find_first_of(':')) == std::string::npos )
			continue;

		name = i->substr(0, n);
		for ( k =0; k<name.size(); k++ ) name[k] = tolower(name[k]);
		arg = i->substr( n+2 );

		if ( name == "host" )
		{
			k = arg.find_first_of(':');
			if ( k == std::string::npos )
			{
				act_h->hostname = arg;
				act_h->port = "80";
			}
			else
			{
				act_h->hostname = arg.substr(0,k);
				act_h->port = arg.substr(k+1);
			}
			msg.pdebug(D_HEADER, "host: \"%s\", port \"%s\"",
					act_h->hostname.c_str(),
					act_h->port.c_str());

		}
		else if ( name == "user-agent" )
		{
			if ( ( k = arg.find("MSIE") ) != std::string::npos )
			{
				act_h->browser = HttpHeader::IE;
				act_h->version = arg.substr(k + 5, 3);
			}
			else if ( ( k = arg.find("Netscape")) != std::string::npos )
			{
				act_h->browser = HttpHeader::NS;
				act_h->version = arg.substr(k + 9,3);
			}
			else if ( ( k = arg.find("Safari")) != std::string::npos)
			{
			    act_h->browser = HttpHeader::SAFARI;
			    act_h->version = arg.substr(k-6, 1);
			}
			else if ( ( k = arg.find("Mozilla")) != std::string::npos )
			{
				act_h->browser = HttpHeader::MOZILLA;
				act_h->version = arg.substr(k+8, 3 );
			}
			else if ( ( k = arg.find("Opera")) != std::string::npos )
			{
				act_h->browser = HttpHeader::OPERA;
				act_h->version = arg.substr(k+6, 4 );
			}
			msg.pdebug(D_HEADER, "browser: \"%d\", version \"%s\"",
					act_h->browser,
					act_h->version.c_str());
		}
		else if ( name == "content-type" )
		{
			act_h->post_type = arg;
			msg.pdebug(D_HEADER, "content_type: %s", act_h->post_type.c_str());
		}
		else if ( name == "content-length" )
		{
			act_h->post_length = atol(arg.c_str());
			msg.pdebug(D_HEADER, "content_length: %d",
					act_h->post_length);
		}
		else if ( name == "connection" )
		{
			act_h->connection = ! ( arg == "close" );
			msg.pdebug(D_HEADER, "connection: \"%d\", \"%s\"",
					act_h->connection,
					act_h->connection ? "alive" : "close");
		}
		else if ( name == "referer" )
		{
			act_h->referer = arg;
			msg.pdebug(D_HEADER, "referer: \"%s\"", arg.c_str());
		}
		else if ( name == "cookie" )
		{
			act_h->cookies.setCookie(arg);
			msg.pdebug(D_HEADER, "cookie: \"%s\"", arg.c_str());
		}
		else if ( name == "authorization" )
		{
			std::string methode;

			if ( ( k = arg.find_last_of(' ') ) == std::string::npos )
			{
				act_h->user = "falsch";
				act_h->passwd = "falsch";
			}
			else
			{
				methode = arg.substr(0, k);
				arg = arg.substr(k+1);

				if ( methode == "Basic")
				{
					CryptBase64 crypt;
					arg = crypt.decode(arg);
					k = arg.find_first_of(':');

					if ( k != std::string::npos )
					{
						act_h->user = arg.substr(0,k);
						act_h->passwd = arg.substr(k+1);
					}
					else
					{
						act_h->user = "falsch";
						act_h->passwd = "falsch";
					}
				}
				else
				{
					msg.perror(E_AUTH_NOT_SUPPORTED,
							"Authorisationsmethode \"%s\" "
							"wird nicht unterstützt",
							methode.c_str());

					act_h->user = "falsch";
					act_h->passwd = "falsch";
				}
				msg.pdebug(D_HEADER, "user: \"%s\"", act_h->user.c_str());
			}
		}

	}

	h->second.clear();
	h->second.push_back("");
}


void HttpAnalyse::read_postvalues()
{
	if ( act_h->post_type.find("multipart/form-data") != std::string::npos )
	{
		std::string::size_type npos;

		if ( ( npos = act_h->post_type.find("boundary=")) != std::string::npos )
			act_h->vars.setMultipart( "--" + act_h->post_type.substr(npos + 9),
					act_h->post_data);
	}
	else
	{
		act_h->vars.setVars(act_h->post_data);
	}

	if ( act_h->post_data != NULL )
	{
		delete act_h->post_data;
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

	for(c = buffer; ( c - buffer ) < size ||
	( act_h != NULL && act_h->needed_postdata == 0); c++ )
	{
		if ( act_h != NULL &&
				act_h->client == client &&
				act_h->needed_postdata > 0 )
		{
			int b_rest;

            msg.pdebug(D_CON, "bekomme noch %d Daten",
                    size);
            msg.pdebug(D_CON, "benötige noch %d Daten",
                    act_h->needed_postdata);

			b_rest = size - ( c - buffer );
			if ( b_rest > act_h->needed_postdata )
				b_rest = act_h->needed_postdata;

            msg.pdebug(D_CON, "lese noch %d Daten",
                    b_rest);

            memcpy(&act_h->post_data[act_h->post_length-act_h->needed_postdata],
					c, b_rest);
			act_h->needed_postdata -= b_rest;
			c = c + b_rest - 1;

			if ( act_h->needed_postdata == 0 )
				read_postvalues();

		}

		else if ( act_h != NULL &&
				act_h->client == client &&
				act_h->needed_postdata == 0 )
		{
			check_user(act_h);

#ifdef PTHREAD
			putHeader(act_h);
#else
			https[0]->get(act_h);
#endif

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

				act_h->clear();
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
	if ( ( h = headers.find(client)) != headers.end() )
	{
		msg.pdebug(D_CON, "Verbindung zum Client %d wurde abgebrochen",client);
		headers.erase(h);
	}
	else
	{
		msg.pdebug(D_CON, "Client %d existiert nicht",client);
	}
}

void HttpAnalyse::add_http(Http *http)
{
#ifdef PTHREAD
	https.push_back(new HttpAnalyseThreadParam(http));
#else
	https = http;
#endif
}

void HttpAnalyse::del_http(Http *http)
{
	msg.pwarning(W_HTTP, "Http kann nicht gelöscht werden nicht finden");
	return;
}

