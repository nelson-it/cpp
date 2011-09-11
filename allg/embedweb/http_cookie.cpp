#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "http_cookie.h"

void HttpCookie::setCookie(std::string cookie)
{
    std::string::size_type cpos, vpos;
    std::string c, name, value;

    cookies.clear();
    while ( cookie != "" )
    {
        cpos = cookie.find_first_of(';');
	if ( cpos != std::string::npos )
	{
	    c = cookie.substr(0, cpos);
	    while ( isspace(cookie[++cpos]));
	    cookie = cookie.substr(cpos);
	}
	else
	{
	    c = cookie;
	    cookie = "";
	}

	if ( ( vpos = c.find_first_of('=')) != std::string::npos )
	{
	    if ( cookies.find(c.substr(0, vpos)) == cookies.end() )
		cookies[c.substr(0, vpos)] = c.substr(vpos + 1);
	}
    }

}

std::string HttpCookie::operator[](const char *name)
{
    Cookies::iterator i;
    if ( ( i = cookies.find(name) ) != cookies.end() )
        return i->second;
    else
        return "";
}

