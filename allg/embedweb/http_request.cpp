#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include <utils/cslist.h>
#include <crypt/base64.h>
#include <crypt/sha1.h>
#include <argument/argument.h>

#include "http_request.h"

HttpRequest::HttpRequest(ServerSocket *s)
            :msg("HttpRequest")
{
	Argument a;
	CsList spath;
	CsList dele;
    unsigned int i;
	std::string projectroot(a["projectroot"]);

	this->serversocket = s;

    port = std::to_string((long)a["port"]);
    sport = std::to_string((long)a["sport"]);

#if defined(__MINGW32__) || defined(__CYGWIN__)
	spath.setString(a["EmbedwebHttpServerpath"], '!');
#else
	spath.setString(a["EmbedwebHttpServerpath"], ':');
#endif
	for (i=0; i<spath.size(); i++)
	{
#if defined(__MINGW32__) || defined(__CYGWIN__)
	    if ( spath[0][1] != ':')
#else
	    if ( spath[0][0] != '/')
#endif
	        serverpath.push_back(projectroot + "/" + spath[i]);
	    else
		    serverpath.push_back(spath[i]);
	}

#if defined(__MINGW32__) || defined(__CYGWIN__)
	spath.setString(a["EmbedwebHttpDatapath"], '!');
#else
	spath.setString(a["EmbedwebHttpDatapath"], ':');
#endif
	for (i=0; i<spath.size(); i++)
	{
	    dele.setString(spath[i],'@');
#if defined(__MINGW32__) || defined(__CYGWIN__)
	    if ( dele[1][1] != ':')
#else
	    if ( dele[1][0] != '/')
#endif
		    datapath[dele[0]] = projectroot + "/" + dele[1];
	    else
		    datapath[dele[0]] = dele[1];
	}

    this->dataroot = std::string(a["EmbedwebHttpDataroot"]);
#if defined(__MINGW32__) || defined(__CYGWIN__)
    if ( this->dataroot[1] != ':' )
    {
#else
    if ( this->dataroot[0] != '/' )
    {
#endif
        this->dataroot = projectroot + "/" + this->dataroot;
    }
    content_types["gif"]  = "image/gif";
    content_types["png"]  = "image/png";
    content_types["jpg"]  = "image/jpeg";
    content_types["jpe"]  = "image/jpeg";
    content_types["jpeg"] = "image/jpeg";
    content_types["ico"]  = "image/x-icon";
    content_types["svg"]  = "image/svg+xml";

	content_types["txt"]  = "text/plain";
	content_types["html"] = "text/html";
	content_types["xml"]  = "text/xml";
	content_types["css"]  = "text/css";
	content_types["js"]   = "text/javascript";
	content_types["mjs"]  = "text/javascript";

	content_types["gz"]   = "application/gzip";
	content_types["gtar"] = "application/x-gtar";
	content_types["z"]    = "application/x-compress";
	content_types["zip"]  = "application/zip";

	content_types["pdf"]  = "application/pdf";
	content_types["ai"]   = "application/postscript";
	content_types["eps"]  = "application/postscript";
	content_types["ps"]   = "application/postscript";

	content_types["mp3"]   = "audio/mpeg";
	content_types["ogg"]   = "audio/ogg";
	content_types["flac"]  = "audio/flac";

}

HttpRequest::~HttpRequest()
{
}

void HttpRequest::analyse_requestline( std::string arg, HttpHeader *h )
{
    Argument a;
	std::string::size_type n;

	h->serverpath = serverpath;
	h->datapath = datapath;
	h->dataroot = dataroot;

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

		while ( ( n = request.find("//")) != std::string::npos )
		    request.replace(n, 2, "/");

		if ( request[request.length()-1] == '/' )
		    request = request.substr(0, request.length() - 1);

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
			h->filename = ( request != "" ) ? request : (std::string)a["EmbedwebHttpIndex"];
		}

		if ( h->filename.find_last_of('.') == std::string::npos )
		{
			h->dirname = h->dirname + "/" + h->filename;
			h->filename = "index.html";
		}

	    if ( ( n = h->filename.find_last_of('.')) != std::string::npos )
	    {
	        ContentTypes::iterator c;

	        if ( ( c = content_types.find(h->filename.substr(n+1)) ) != content_types.end() )
	            h->content_type = c->second;
	    }
	}
	else
	{
		msg.perror(E_REQUEST, "Anforderungszeile %s ist im falschen Format", arg.c_str());
		return;
	}
}

void HttpRequest::analyse_hostline( std::string arg, HttpHeader *h )
{
    std::string::size_type k;

    if ( arg[0] != '[')
    {
        k = arg.find_first_of(':');
        if ( k == std::string::npos )
        {
            h->hostname = arg;
            h->port = "80";
        }
        else
        {
            h->hostname = arg.substr(0,k);
            h->port = arg.substr(k+1);
        }
    }
    else
    {
        k = arg.find_first_of(']');
        h->hostname = arg.substr(0, k + 1);
        if ( ( k = arg.find_first_of(':', k )) == std::string::npos)
            h->port = "80";
        else
            h->port = arg.substr(k+1);
    }
}

void HttpRequest::analyse_authline( std::string arg, HttpHeader *h )
{
    std::string::size_type k;

    std::string methode;

    if ( ( k = arg.find_last_of(' ') ) == std::string::npos )
    {
        h->user = "falsch";
        h->passwd = "falsch";
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
                h->user = arg.substr(0,k);
                h->passwd = arg.substr(k+1);
            }
            else
            {
                h->user = "falsch";
                h->passwd = "falsch";
            }
        }
        else
        {
            msg.perror(E_AUTH_NOT_SUPPORTED, "Authorisationsmethode \"%s\" " "wird nicht unterstützt", methode.c_str());

            h->user = "falsch";
            h->passwd = "falsch";
        }
    }
}

void HttpRequest::analyse_header(Header *h, HttpHeader *act_h)
{
	Header::iterator i;

	std::string name;
	std::string arg;
	std::string::size_type n,k;
	std::string forward_addr("-"), forward_host("-"), forward_port("-"), forward_proto("-");

	if ( h->size() < 2 )
	{
		msg.perror(E_REQUEST_TO_SHORT, "Zu wenig Zeilen im Request");
		return;
	}

	// Headers auswerten
	// ==========================

	for( i = h->begin(); i != h->end(); ++i )
	{

		if ( ( n = i->find_first_of(" :")) == std::string::npos )
			continue;

		name = i->substr(0, n);
		for ( k =0; k<name.size(); k++ ) name[k] = tolower(name[k]);
		arg = i->substr( n+1 );
		while ( arg[0] == ' ') arg = arg.substr(1);

		//act_h->rawheader[i->substr(0, n)] = arg;
        msg.pdebug(D_RAWHEADER, " Header %s:%s", name.c_str(), arg.c_str());
		act_h->header[name] = arg;

		if ( name == "get")
		{
		    act_h->typ = "GET";
		    analyse_requestline( arg, act_h );
         	msg.pdebug(1, "GET ordner: \"%s\" datei: \"%s\", protokoll \"%s\"", act_h->dirname.c_str(), act_h->filename.c_str(), act_h->protokoll.c_str());
		}
		if ( name == "post")
		{
		    act_h->typ = "POST";
		    analyse_requestline( arg, act_h );
         	msg.pdebug(1, "POST ordner: \"%s\" datei: \"%s\", protokoll \"%s\"", act_h->dirname.c_str(), act_h->filename.c_str(), act_h->protokoll.c_str());
		}
		else if ( name == "host" )
		{
	        analyse_hostline( arg, act_h );
		}
		else if ( name == "content-type" )
		{
			act_h->post_type = arg;
			msg.pdebug(D_HEADER, "content_type: %s", act_h->post_type.c_str());
		}
		else if ( name == "content-length" )
		{
			act_h->post_length = atol(arg.c_str());
			msg.pdebug(D_HEADER, "content_length: %d", act_h->post_length);
		}
		else if ( name == "connection" )
		{
			act_h->connection = ! ( arg == "close" );
			msg.pdebug(D_HEADER, "connection: \"%d\", \"%s\"", act_h->connection, act_h->connection ? "alive" : "close");
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
	        analyse_authline( arg, act_h );
			msg.pdebug(D_HEADER, "user: \"%s\"", act_h->user.c_str());
		}
        else if ( name == "x-forwarded-server")
        {
            CsList l(arg);
            forward_host = l[0];
        }
        else if ( name == "x-forwarded-for")
        {
                forward_addr = arg;
		        act_h->client_host = arg;
        }
		else if ( name == "x-forwarded-proto")
		{
		        forward_proto = arg;
		}
		else if ( name == "x-forwarded-port")
		{
		        forward_port = arg;
		}

		else if  ( name == "sec-websocket-key" && this->serversocket != NULL )
		{
		    Sha1 sha;
		    CryptBase64 base64;
            unsigned char buffer[SHA1HashSize];
            unsigned char out[128];

		    std::string str = arg + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		    sha.set((const uint8_t*) str.c_str(), str.length());
		    sha.get(buffer);

		    out[base64.encode(buffer, out, SHA1HashSize )-1] = '\0';

		    act_h->extra_header.push_back("Upgrade: websocket");
		    act_h->extra_header.push_back("Connection: Upgrade");
		    act_h->extra_header.push_back(std::string("Sec-WebSocket-Accept: ") + std::string((char*)out));
		    act_h->extra_header.push_back("Sec-WebSocket-Protocol: mneserver");
		    act_h->status = 101;

		    this->serversocket->setProvider(act_h->client, "Ws");

		}
	}

	h->clear();
	h->push_back("");

	if ( this->serversocket != 0 )
	    this->serversocket->setAddr(act_h->client, forward_addr, forward_port );

	act_h->base = (( forward_proto == "-" ) ? (( act_h->port == sport ) ? "https" : "http") : forward_proto ) + "://";
	act_h->base = act_h->base + (( forward_host == "-" ) ? act_h->hostname : forward_host);
	act_h->base = act_h->base + (( forward_port == "-" ) ? ((act_h->port != "80" ) ? ( ":" + act_h->port ) : "" )  : ( forward_port != "" ) ? ( ":" + forward_port ) : "" );
	if ( act_h->base[act_h->base.length() - 1] == '/') act_h->base = act_h->base.substr(0,act_h->base.length() - 1 );
	msg.pdebug(D_HEADER, "base: \"%s\"", act_h->base.c_str());
}
