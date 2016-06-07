#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <argument/argument.h>
#include "http.h"

#include "http_translate.h"

HttpTranslate::HttpTranslate()
:msg("HttpTranslate")
{
}

HttpTranslate::~HttpTranslate()
{
}

void HttpTranslate::make_answer(HttpHeader *act_h, FILE *file )
{
	char *buffer;
	int size;
	char *c, *old_c;
	std::string str;
	FILE *f;
	Argument a;

	if ( file != NULL ) f = file;
	else f = act_h->content;

	fseek( f, 0, SEEK_END);
	size = ftell(f);
	if ( size < 0 ) msg.perror(1, "Fehler bei ftell");
	fseek( f, 0, SEEK_SET);

	if ( size < 0 ) size = 0;

	buffer = new char[size + 1];
	buffer[size] = '\0';
	fread(buffer, size, 1, f);

	fclose(act_h->content);
	act_h->content = fopen(act_h->content_filename.c_str(), "wbe+");
	fseek ( act_h->content, 0, SEEK_SET);

	old_c = buffer;

	while ( ( c = strstr(old_c, "#") ) != NULL )
	{
		fwrite(old_c, c - old_c, 1, act_h->content);

		str.clear();
		c++;
		old_c = c;
		while( *c != '#' && *c != '\0' )
		{
			str.push_back(*c);
			c++;
		}

		if ( *c != '\0' )
		{
			if ( str == "mne_lang" )
			{
				str.clear();
				c++;
				while ( *c != '"' &&
						* c != '\'' &&
						* c != '<' &&
						* c != '>' &&
						* c != '#' &&
						*c != '\0' )
				{
					if ( *c == '\\' ) c++;
					if ( *c == '\0' ) break;

					str.push_back(*c);
					c++;
				}

				if ( *c == '#') c++;
				str = msg.get(str);

				old_c = c;
			}
			else if ( str == "mne_base" )
			{
			    str = act_h->base + "/";
			    c++;
			    old_c = c;
			}
			else if ( act_h->vars.exists(str) )
			{
				str = act_h->vars[str];
				c++;
				old_c = c;
			}
			else
			{
				switch(a.exists(str.c_str()))
				{
				case 'c' :
					str = std::string(a[str.c_str()]);
					c++;
					old_c = c;
					break;
				case 'l' :
				{
					char buffer[256];
					sprintf(buffer, "%ld", (long)a[str.c_str()]);
					str = buffer;
					c++;
					old_c = c;
					break;
				}
				case 'd' :
				{
					char buffer[256];
					sprintf(buffer, "%f", (double)a[str.c_str()]);
					str = buffer;
					c++;
					old_c = c;
					break;
				}
				default:
					str = "#";
					c++;
					break;
				}
			}
		}
		else
		{
			str = "#" + str;
			old_c = c;
		}

		fwrite(str.c_str(), str.size(), 1, act_h->content);
	}

	fwrite(old_c, ( buffer + size ) - old_c, 1, act_h->content);

	delete[] buffer;
	return;

}

