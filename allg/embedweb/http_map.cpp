#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <argument/argument.h>
#include "http_map.h"

HttpMap::HttpMap(Http *h, std::string path, std::string origpath,
                          std::string files)
          :msg("HttpMap")
{
    Argument a;
    this->path = path;
    this->origpath = origpath;

    std::string root = (char *)a["projectroot"];

    if ( origpath[0] != '/' && origpath[1] != ':' && origpath[0] != '\\' )
    	this->origpath = root + '/' + this->origpath;

    this->files.setString(files,'~');

    h->add_provider(this);
}

HttpMap::~HttpMap()
{
}

int HttpMap::request(HttpHeader *h)
{

    if ( ! files.empty() && files.find(h->filename) == std::string::npos )
        return 0;

    h->serverpath.clear();
    h->serverpath.push_back(origpath);

    if ( h->dirname[0] == '/' )
	h->dirname = h->dirname.substr(1);

    h->status = 1000;
    return 1;
}

std::string HttpMap::mkfilename(HttpHeader *h, std::string request)
{

    std::string filename;
    std::string::size_type n;

    if ( ! files.empty() )
    {
	if ( ( n = request.find_last_of('/') ) != std::string::npos )
	{
		filename = request.substr(n + 1);

	    if ( filename == "" )
		filename = "index.html";
	}
	else
	{
	    filename = request;
	}

	if ( filename.find_last_of('.') == std::string::npos )
	    filename = "index.html";

	if ( files.find(filename) == std::string::npos )
	    return "";

    }

    return ( origpath + request.substr(path.size() + 1));
}

