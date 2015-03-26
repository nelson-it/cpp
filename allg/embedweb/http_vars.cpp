#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <windows.h>
#include <sec_api/stdio_s.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#include <crypt/base64.h>
#include "http_vars.h"

HttpVars::~HttpVars()
{
    clear();
}

void
HttpVars::clear()
{
    Files::iterator i;

    for (i = files.begin(); i != files.end(); ++i)
#if defined(__MINGW32__) || defined(__CYGWIN__)
        DeleteFile(i->second.c_str());
#else
    unlink(i->second.c_str());
#endif

    vars.clear();
    files.clear();
}

void
HttpVars::clear(const char *name)
{
    Vars::iterator i;
    if ((i = vars.find(name)) != vars.end()) i->second = "";
}

void
HttpVars::setVars(std::string vars)
{
    std::string::size_type cpos, vpos;
    std::string c, name, value;

    msg.pdebug(5, "%s", vars.c_str());
    while (vars != "")
    {
        cpos = vars.find_first_of('&');
        if (cpos != std::string::npos)
        {
            c = vars.substr(0, cpos);
            vars = vars.substr(cpos + 1);
        }
        else
        {
            c = vars;
            vars = "";
        }

        if ((vpos = c.find_first_of('=')) != std::string::npos)
        {
            std::string val = c.substr(vpos + 1);

            while ((cpos = val.find('+')) != std::string::npos)
                val.replace(cpos, 1, " ", 1);

            cpos = 0;
            while ((cpos = val.find('%', cpos)) != std::string::npos)
            {
                char cv[2];

                cv[0] = (char) strtol(val.substr(cpos + 1, 2).c_str(), NULL, 16);
                cv[1] = '\0';

                val.replace(cpos, 3, cv);
                cpos++;
            }

            this->vars[c.substr(0, vpos)] = val;
        }
    }

    Vars::iterator v;
    for (v = this->vars.begin(); v != this->vars.end(); ++v)
        msg.pdebug(5, "name: \"%s\" wert: \"%s\"", v->first.c_str(),
                v->second.c_str());

}

void
HttpVars::setMultipart(std::string boundary, char *data)
{
    std::string::size_type npos;

    std::string name;
    std::string value;
    std::string str;
    std::string content_type;

    char *c_old, *c;

    for (c = data; *c != '\0'; str.clear())
    {

        // Zeile lesen
        for (c_old = c; *c != '\r' && *c != '\n' && *c != '\0'; c++)
            ;
        str.insert(0, c_old, c - c_old);

        if (*c == '\r')
        {
            c++;
            c++;
        }

        if (str == boundary)
        {
            name.clear();
            value.clear();
            content_type.clear();

            continue;
        }
        else if (str == boundary + "--")
        {
            return;
        }
        else if (str == "") // ab hier beginnen die Daten
        {
            for (c_old = c; strncmp(c, boundary.c_str(), boundary.size()) != 0; c++);

            if (content_type != "")
            {
                FILE *f;
                //char str[32];
				char filename[512];
#if defined(__MINGW32__) || defined(__CYGWIN__)
				*filename = '\0';
				if ( getenv ("TEMP") != NULL)
				{
					strncpy(filename, getenv("TEMP"), sizeof(filename) -1 );
					strncat(filename, "\\HttpVarsXXXXXX", sizeof(filename) - strlen(str) - 1);
				}
				_mktemp_s(filename, strlen(filename) + 1);
				filename[sizeof(filename) - 1] = '\0';
                f = fopen(tmp, "wb"));
#else
                int fd;
                strcpy(filename, "/tmp/HttpVarsXXXXXX");
                fd = mkstemp(filename);
                if ( fd >= 0 )
                {
                    if ( ( f = fdopen(fd, "wb+") ) == NULL )
                        close(fd);
                }
                else
                {
                    f = NULL;
                }

#endif
                if ( f == NULL )
                {
                	msg.perror(FILEOPEN, "konnte temporäre Datei %s nicht öffnen", filename);
                }
                else
                {
                    fwrite(c_old, 1, c - c_old - 2, f);
                    fclose(f);
                    files[name] = filename;
                    vars[name] = "##########" + content_type;
                }
            }
            else
            {
                char *tmpstr;
                tmpstr = new char[c - c_old];
                strncpy(tmpstr, c_old, c - c_old - 2);
                tmpstr[c - c_old - 2] = '\0';
                vars[name] = url_decode(tmpstr);
                delete tmpstr;

            }
        }
        else if (str.find("Content-Disposition:") != std::string::npos)
        {
            if ((npos = str.find("name")) == std::string::npos) continue;

            name = str.substr(npos);
            if ((npos = name.find_first_of(';')) != std::string::npos)
            {
                value = name.substr(npos + 1);
                name = name.substr(6, npos - 7);
                npos = value.find("filename");
                if (npos != std::string::npos) value = value.substr(npos + 10, value.size() - 12);
                else value = "";
                if ( value.rfind('\\') != std::string::npos )
                    value = value.substr(value.rfind('\\') + 1);
            }
            else
            {
                name = name.substr(6, name.size() - 7);
            }
        }
        else if (str.find("Content-Type:") != std::string::npos)
        {
            content_type = str.substr(14);
            continue;
        }
        else
        {
            while (strncmp(c, boundary.c_str(), boundary.size()) != 0)
                c++;
        }
    }
}

std::string HttpVars::operator[](const char *name )
{
    Vars::iterator i;
    if ((i = vars.find(name)) != vars.end()) return i->second;
    else return "";
}

int HttpVars::exists(const char *name)
{
    if (vars.find(name) != vars.end()) return 1;
    else return 0;
}

std::string HttpVars::getFile(const char *name )
{
    Files::iterator i;
    if ((i = files.find(name)) != files.end()) return i->second;
    else return "";
}

std::string HttpVars::data(const char *name, int raw)
{
    Vars::iterator i;
    int f;

    if ((i = files.find(name)) != files.end())
    {
        if ((f = open(i->second.c_str(), O_RDONLY)) < 0 )
        {
            msg.perror(FILEOPEN,
                    "konnte temporäre Datei %s nicht öffnen", i->second.c_str());
            return "";
        }
        else if ( raw == 0 )
        {
            CryptBase64 base64;
            int size;
            int i;
            int length;

            size = lseek(f, 0, SEEK_END);
            lseek(f, 0, SEEK_SET);

            unsigned char *in = (unsigned char *)new char[size];
            unsigned char *out = (unsigned char *)new char[base64.encode_needsize(size) + 1];

            i = 0;
            length=0;
            while ( ( i = read(f, &(in[length]), size - length )) > 0 )
                length += i;
            close(f);

            out[base64.encode(in, out, size)] = '\0';
            std::string retval((char*)out);

            delete in;
            delete out;

            return retval;
        }
        else
        {
            int size;
            size = lseek(f, 0, SEEK_END);
            lseek(f, 0, SEEK_SET);

            unsigned char *in = (unsigned char *)new char[size + 1];

            read(f, in, size);
            close(f);

            in[size] = '\0';
            std::string retval((char*)in);
            delete in;

            return retval;
        }
    }
    else return "";
}

std::string HttpVars::url_decode(std::string val)
{
    std::string::size_type cpos = 0;

    while ((cpos = val.find('%', cpos)) != std::string::npos)
    {
        char cv[2];

        cv[0] = (char) strtol(val.substr(cpos + 1, 2).c_str(), NULL, 16);
        cv[1] = '\0';

        val.replace(cpos, 3, cv);
        cpos++;
    }
    return val;

}

