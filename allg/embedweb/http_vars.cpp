#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <winsock2.h>
#include <windows.h>
#include <sec_api/stdio_s.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#include <crypt/base64.h>
#include "http_vars.h"


int percent_decode(char *out, const char *in)
{
    static const unsigned char tbl[256] =
    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 10, 11, 12, 13, 14, 15, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 10, 11, 12, 13, 14, 15, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff };
    char c, v1, v2, *beg = out;
    if (in != NULL)
    {
        while ((c = *in++) != '\0')
        {
            if (c == '%')
            {
                if ((v1 = tbl[(unsigned char) *in++]) < 0 || (v2 =
                        tbl[(unsigned char) *in++]) < 0)
                {
                    *beg = '\0';
                    return -1;
                }
                c = (v1 << 4) | v2;
            }
            *out++ = c;
        }
    }
    *out = '\0';
    return 0;
}

HttpVarsMultipart::HttpVarsMultipart(HttpVars *vars, std::string boundary)
{
    this->vars = vars;
    this->boundary = boundary;

    wd = NULL;
    startfound = 0;
    endfound = 0;
    isvalue = 0;
    waitfile = 0;
    bufpos = 0;
    buflen = 0;
    *filename = '\0';

    bufsize = 10240;
    buf = new char[bufsize];
}

HttpVarsMultipart::~HttpVarsMultipart()
{
    delete[] buf;
    if ( wd != NULL )
    {
        fclose(wd);
        unlink(filename);
    }
}

FILE *HttpVarsMultipart::opentmp(char *filename)
{
    FILE *f;
#if defined(__MINGW32__) || defined(__CYGWIN__)
    *filename = '\0';
    if ( getenv ("TEMP") != NULL)
    {
        strncpy(filename, getenv("TEMP"), sizeof(filename) -1 );
        strncat(filename, "\\HttpVarsXXXXXX", sizeof(filename) - strlen(filename) - 1);
    }
    _mktemp_s(filename, strlen(filename) + 1);
    filename[sizeof(filename) - 1] = '\0';
    f = fopen(filename, "wb");
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
    if (f == NULL)
        vars->msg.perror(HttpVars::E_FILEOPEN, "konnte temporäre Datei %s nicht öffnen", filename);

    return f;
}


void HttpVarsMultipart::putfile()
{
    if ( wd == NULL )
        wd = opentmp(filename);

    int oldbufpos;
    while ( waitfile && bufpos < buflen )
    {
        for (oldbufpos = bufpos; bufpos != buflen && buf[bufpos] != '\r'; bufpos++);
        fwrite(&buf[oldbufpos], bufpos - oldbufpos, 1, wd);
        memmove(buf, &buf[bufpos], buflen - bufpos);
        buflen -= bufpos;
        bufpos = 0;

        if (buflen - bufpos < (int)boundary.size() + 2)
        {
            return;
        }
        else if (strncmp(&buf[bufpos + 2], boundary.c_str(), boundary.size()) == 0)
        {
            vars->msg.pdebug(5, "name: %s filename: %s type %s", name.c_str(), filename, content_type.c_str());

            if ( wd != NULL ) fclose(wd);
            vars->files[name] = filename;
            vars->setVar(name, "##########" + content_type);

            wd = NULL;
            waitfile = 0;

            memmove(buf, &buf[2], buflen - 2);
            buflen -= 2;
            bufpos = 0;

            return;
        }
        else
        {
            if ( wd != NULL ) fwrite(&buf[bufpos], 1, 1, wd );
            bufpos++;
        }
    }
}

void HttpVarsMultipart::putline()
{
    while ( bufpos < buflen )
    {

        for (; bufpos < buflen && buf[bufpos] != '\r' && buf[bufpos] != '\n'; bufpos++) str.push_back(buf[bufpos]);

        if (bufpos == buflen) return;
        if ( buf[bufpos] == '\r' || buf[bufpos] == '\n' ) bufpos++;
        if (bufpos == buflen) return;
        if ( buf[bufpos] == '\r' || buf[bufpos] == '\n' ) bufpos++;

        if (str == boundary )
        {
            name.clear();
            value.clear();
            content_type.clear();
            str.clear();
            startfound = 1;

            continue;
        }
        else if (str == boundary + "--")
        {
            endfound = 1;
            break;
        }
        else if ( startfound )
        {

            if (isvalue )
            {
                vars->msg.pdebug(5, "name: %s, wert: %s\n", name.c_str(), vars->url_decode(str).c_str());
                vars->setVar(name, vars->url_decode(str));
                isvalue = 0;
                str.clear();
                startfound = 0;
            }
            else if (str.find("Content-Disposition:") != std::string::npos)
            {
                std::string::size_type npos;
                if ((npos = str.find("name")) == std::string::npos) continue;

                name = str.substr(npos);
                if ((npos = name.find_first_of(';')) != std::string::npos)
                {
                    value = name.substr(npos + 1);
                    name = name.substr(6, npos - 7);
                    npos = value.find("filename");
                    if (npos != std::string::npos) value = value.substr(npos + 10, value.size() - 12);
                    else value = "";

                    if (value.rfind('\\') != std::string::npos) value = value.substr(value.rfind('\\') + 1);
                }
                else
                {
                    name = name.substr(6, name.size() - 7);
                }
                str.clear();
            }
            else if (str.find("Content-Type:") != std::string::npos)
            {
                content_type = str.substr(14);
                str.clear();
                continue;
            }
            else if ( str == "" && content_type != "" )
            {
                waitfile = 1;
                return;
            }
            else if ( str == "" )
            {
                isvalue = 1;
            }
        }
        else
        {
            vars->msg.perror(HttpVars::E_REQUEST, "Fehler im Request");
            return;
        }
    }
}

void HttpVarsMultipart::put(char *b, int size)
{
    if ( bufsize < buflen + size )
    {
        char *tmp = new char[buflen + size];
        memcpy(tmp, buf, buflen);
        delete[] buf;
        buf = tmp;
        bufsize = buflen + size;
    }

    if ( size != 0 ) memcpy(&buf[buflen], b, size);
    buflen += size;

    if ( waitfile )
    {
        putfile();
    }
    else
    {
        putline();
        if ( waitfile )
        {
            putfile();
        }
    }
}


HttpVars::~HttpVars()
{
    clear();
}

void HttpVars::clear()
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

    if ( multipart != NULL )
        delete multipart;
    multipart = NULL;
}

void HttpVars::clear(const char *name)
{
    Vars::iterator i;
    if ((i = vars.find(name)) != vars.end()) i->second = "";
}


void HttpVars::setVars(std::string vars)
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
            char *result;

            while ((cpos = val.find('+')) != std::string::npos)
                val.replace(cpos, 1, " ", 1);

            result = new char[val.size() + 1];
            if (percent_decode(result, val.c_str()) == 0)
            {
                val = "";
                this->vars[c.substr(0, vpos)] = std::string(result);
                delete[] result;
            }
            else
            {
                msg.perror(E_DECODE, "Decodefehler für Variable %s", c.substr(0, vpos).c_str());
            }
        }
    }

    Vars::iterator v;
    for (v = this->vars.begin(); v != this->vars.end(); ++v)
        msg.pdebug(5, "name: \"%s\" wert: \"%s\"", v->first.c_str(),
                v->second.c_str());
}

void HttpVars::setMultipart(std::string boundary, char *value, int size)
{
    if ( multipart == NULL )
        multipart = new HttpVarsMultipart(this, boundary);

    multipart->put(value, size);

    if ( size == 0 )
    {
        if ( ! multipart->isready() )
            msg.perror(E_REQUEST, "Fehler im Request");

        delete multipart;
        multipart = NULL;
    }

}

std::string HttpVars::operator[](const char *name)
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

std::string HttpVars::getFile(const char *name)
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
        if ((f = open(i->second.c_str(), O_RDONLY)) < 0)
        {
            msg.perror(E_FILEOPEN, "konnte temporäre Datei %s nicht öffnen", i->second.c_str());
            return "";
        }
        else if (raw == 0)
        {
            CryptBase64 base64;
            int size;
            int i;
            int length;

            size = lseek(f, 0, SEEK_END);
            lseek(f, 0, SEEK_SET);

            unsigned char *in = (unsigned char*) new char[size];
            unsigned char *out =
                    (unsigned char*) new char[base64.encode_needsize(size) + 1];

            i = 0;
            length = 0;
            while ((i = read(f, &(in[length]), size - length)) > 0)
                length += i;
            close(f);

            out[base64.encode(in, out, size)] = '\0';
            std::string retval((char*) out);

            delete[] in;
            delete[] out;

            return retval;
        }
        else
        {
            int size;
            size = lseek(f, 0, SEEK_END);
            lseek(f, 0, SEEK_SET);

            unsigned char *in = (unsigned char*) new char[size + 1];

            read(f, in, size);
            close(f);

            in[size] = '\0';
            std::string retval((char*) in);
            delete[] in;

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

