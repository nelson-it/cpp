#ifndef http_header_mne
#define http_header_mne

#include <string>
#include <vector>

#include "http_cookie.h"
#include "http_vars.h"

class HttpHeader
{
public:
    enum Browser
    {
        UNKNWON,
	MOZILLA,
	OPERA,
	NS,
	SAFARI,
	IE
    };

    typedef std::map<std::string, std::string> SetCookies;

    std::vector<std::string> serverpath;

    // Wird gesendet
    // =============
    HttpCookie  cookies;
    HttpVars    vars;

    std::string id;

    std::string typ;
    std::string providerpath;
    std::string dirname;
    std::string filename;
    std::string protokoll;

    std::string hostname;
    std::string port;
    std::string referer;

    Browser     browser;
    std::string version;

    std::string user;
    std::string passwd;

    std::string post_type;
    char       *post_data;
    int         post_length;
    int         needed_postdata;

    void       *user_data;
    // Vom Provider auszuf|llen
    // ========================
    int status;
    int translate;
    int age;
    int connection;

    std::string location;
    std::string realm;
    std::string content_filename;
    std::string content_type;
    std::string charset;

    FILE       *content;
    long        content_length;

    SetCookies set_cookies;

    std::vector<std::string> extra_header;

    std::vector<std::string> error_messages;
    int error_found;
    int warning_found;
    int message_found;

public:
    HttpHeader()
    {
    content = NULL;
	post_data = NULL;
	user_data = NULL;
	clear();
    }
    ~HttpHeader()
    {
        if ( content != NULL )   fclose(content);
	if ( post_data != NULL ) delete post_data;
	if ( user_data != NULL ) delete (char *)user_data;
    }

    void clear()
    {
	client = -1;

	id = "";
	serverpath.clear();
	typ       = "";
	providerpath = "";
	dirname   = "";
	filename  = "";
	protokoll = "";

	hostname = "";
	port     = "";
	referer  = "";

	browser = UNKNWON;
	version = "";

	user   = "";
	passwd = "";

    if ( content   != NULL ) fclose(content);
	if ( post_data != NULL ) delete post_data;
    if ( user_data != NULL ) delete (char*)user_data;

	post_data = NULL;
        post_length = 0;
	needed_postdata = -1;

	status = 404;
	translate = 0;
	age = 0;
	connection = 1;

	realm = "";
	location = "";
	content_filename = "";
	content_type   = "text/html";
	charset        = "UTF-8";
	content        = NULL;
	content_length = 0;

	cookies.clear();
	set_cookies.clear();
	vars.clear();

	extra_header.clear();
	error_messages.clear();

	error_found = 0;
	warning_found = 0;
	message_found = 0;

    }

    int client;

};

#endif /* http_header_mne */
