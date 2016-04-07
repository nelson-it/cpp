#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <argument/argument.h>
#include <utils/php_exec.h>

#include "http_search.h"

HttpSearchPath::HttpSearchPath(Http *h, std::string path, int noadd ) :
	HttpProvider(h), msg("HttpSearchPath")
{
	Argument a;
	std::string str;
	std::string::size_type i, ii;

	this->path = path;

	str = std::string(a[((std::string)"EmbedwebHttpSearchPath" + path).c_str()]);
	msg.pdebug(5, "path: %s", str.c_str());

	ii = i = 0;
	while ( (i = str.find(':', i)) != std::string::npos)
	{
		search.push_back(str.substr(ii, i - ii));
		msg.pdebug(3, "%s-searchpath: %s", path.c_str(), str.substr(ii, i-ii).c_str());
		ii = i + 1;
		i++;
	}
	search.push_back(str.substr(ii));
	msg.pdebug(3, "%s-searchpath: %s", path.c_str(), str.substr(ii).c_str());

	if ( ! noadd )
	    h->add_provider(this);
}

HttpSearchPath::~HttpSearchPath()
{
}

int HttpSearchPath::request(HttpHeader *h)
{
	struct stat s;
	std::string file;
	std::string name;
	unsigned int i;

	if ( h->dirname != "/" )
	  name = h->dirname + "/" + h->filename;
	else
	  name = "/" + h->filename;

	for (i=0; i<search.size(); ++i)
	{
		file = http->mkmap(search[i]) + name;
		msg.pdebug(3, "check %s", file.c_str());
		if (stat(file.c_str(), &s) == 0)
		{
		    h->status = 200;
		    if (file.find(".php") == (file.size() - 4 ))
		    {
		        h->age = 0;
		        PhpExec(file, h);
		        return 1;
		    }
		    else
		    {
		        FILE *c = fopen(file.c_str(), "rb");
		        if ( c != NULL )
		        {
		            h->status = 200;
		            fclose(h->content);
		            h->content = c;
		            h->translate = 1;
		            fseek(c, 0, SEEK_END);
		            return 1;
		        }
		        else
		        {
		            if ( h->vars["ignore_notfound"] == "" )
		                msg.perror(E_FILE_OPEN, "kann datei <%s> nicht öffnen", name.c_str());
		            return 0;
		        }
		    }
        }
	}
	if ( h->vars["ignore_notfound"] == "" )
	    msg.perror(E_FILE_FOUND, "kann datei <%s> nicht finden", name.c_str());
	return 0;
}
