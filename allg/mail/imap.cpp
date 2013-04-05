#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <iconv.h>

#include <sys/types.h>
#if defined(__MINGW32__) || defined(__CYGWIN__)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <fcntl.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#ifdef Darwin
#include <xlocale.h>
#endif

#include <crypt/base64.h>
#include <utils/cslist.h>

#include "imap.h"

Imap::Imap(std::string server, std::string user, std::string passwd) :
    msg("Imap")
{
    sock = -1;
    response = NULL;
    connect(server, user, passwd);
}

Imap::~Imap()
{
    if ( sock >= 0 )
    {
        char tmp[1024];
        snprintf(tmp, sizeof(tmp), "%s\r\n", ( this->tag + "logout").c_str());
        write_cmd(tmp, strlen(tmp));
        close( sock);
    }

    if ( response != NULL )
        delete response;

}

int Imap::write_cmd(char *cmd, int len, int need_split )
{
    int  i, count;
    count = 0;

#if defined(__MINGW32__) || defined(__CYGWIN__)
    while (count < len && (i = ::send(sock, &cmd[count], len - count, 0)) > 0)
        count += i;
#else
    while (count < len && (i = write(sock, &cmd[count], len - count)) > 0)
        count += i;
#endif

    if (i < 0)
    {
        msg.perror(E_WRITE, "Fehler beim Schreiben");
        close( sock);
        sock = -1;
        return -1;
    }

    read_answer(need_split);
    return ( sock == - 1 ) ? -1 : 0;
}

void Imap::read_answer(int need_split)
{
    int i, len;
    int notready = 1;
    int size = 10240;

    if ( response != NULL ) delete response;
    response = new char[size + 1];

    memset(response, 0, size + 1);
    answer.clear();

    if (sock < 1) return;

    len = 0;
    while (notready)
    {
#if defined(__MINGW32__) || defined(__CYGWIN__)
        while ((i = ::recv(sock, &response[len], size - len, 0)) > 0)
            len += i;

        if (i < 0 && errno != EAGAIN && errno != 0 )
#else
        while ((i = read(sock, &response[len], size - len)) > 0)
            len += i;

        if (i < 0 && errno != EAGAIN )
#endif
        {
            perror("IMAP");
            msg.perror(E_READ, "Fehler beim lesen");
            close( sock);
            sock = -1;
            return;
        }

#if defined(__MINGW32__) || defined(__CYGWIN__)
        else if ( i < 0 && ( errno == EAGAIN || errno == 0 ) )
#else
        else if ( i < 0 && errno == EAGAIN )
#endif
        {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000;
            select(0,0,0,0,&tv);
        }

        if ( len == size )
        {
            char *s = new char[size + 10241];
            memset(s, 0, size + 10241);
            memcpy(s, response, size);

            delete response;
            response = s;
            size += 10240;
        }
        else if (response[len - 1] == '\n' && response[len - 2] == '\r')
        {
            char *c = ( len > 2 ) ? &response[len - 3] : response;

            while ( *c != '\n' && c != response ) c--;

            if ( c != response ) c++;

            if ( ( strncmp(c, (this->tag + "OK").c_str(),  (this->tag + "OK").size()) == 0 ) ||
                    ( strncmp(c, (this->tag + "NO").c_str(),  (this->tag + "NO").size()) == 0 ) ||
                    ( strncmp(c, (this->tag + "BAD").c_str(), (this->tag + "BAD").size()) == 0) )
                notready = 0;
            else
                notready++;

            if ( ( notready == 0 || notready > 1000 ) && strncmp(c, (this->tag + "NO").c_str(), (this->tag + "NO").size()) == 0)
            {
                msg.pwarning(E_READ, "Unerwartete Anwort");
                msg.line("%s", response);
            }

            else if ( ( notready == 0 || notready > 1000 ) && strncmp(c, (this->tag + "OK").c_str(), (this->tag + "OK").size()) != 0)
            {
                msg.perror(E_READ, "Unerwartete Anwort - breche Verbindung ab");
                msg.line("%s", (this->tag + "OK").c_str());
                msg.line("%s", response);
                close( sock);
                sock = -1;
                return;
            }
        }
    }

    msg.pdebug(10,"%s", response);
    if ( need_split )
    {
        CsList a(response, '\n', 1);
        for ( i = 0; i< (int)a.size(); i++)
        {
            if ( a[i][0] == ' ' )
                answer[answer.size()-1] = answer[answer.size()-1] + a[i].substr(1,a[i].size() - 2);
            else
                answer.push_back(a[i].substr(0,a[i].size() - 1));
        }
    }
}

std::string Imap::getBracketvalue(std::string s, const char *delimiter)
{
    std::string::size_type i,j;
    int k;

    bracket_found = 0;
    k = 0;
    i = s.find(delimiter[0]);
    if ( i != std::string::npos )
    {
        j = s.find_first_of(delimiter,i+1);
        k++;
        while ( k != 0 &&  j != std::string::npos )
        {
            if ( s[j] == delimiter[0] ) k++; else k--;
            if ( k == 0 ) { bracket_found = 1; return s.substr(i+1, j-i-1); }
            j = s.find_first_of(delimiter,j+1);
        }

        if ( j == std::string::npos )
            return s;
    }
    return s;
}

void Imap::connect(std::string server, std::string user, std::string passwd)
{
    char tmp[10240];
#if defined(__MINGW32__) || defined(__CYGWIN__) || defined(Darwin)
    struct hostent *hp;
#else
    struct hostent hostbuf, *hp;
    int herr;
#endif
    struct sockaddr_in s_in;
    int port = 143;

    if (server == "") return;

    CsList s(server, ':');
    if (s.size() > 1) port = atoi(s[1].c_str());

    if ( sock >= 0 )
    {
        snprintf(tmp, sizeof(tmp), "%s\r\n", ( this->tag + "logout").c_str());
        write_cmd(tmp, strlen(tmp));
        close( sock);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        msg.perror(E_OPEN, "Konnte Socket nicht öffnen");
        return;
    }

    s_in.sin_family = AF_INET;
#if defined(__MINGW32__) || defined(__CYGWIN__) || defined(Darwin)
    hp = gethostbyname(s[0].c_str());
#else
    gethostbyname_r(s[0].c_str(), &hostbuf, tmp, sizeof(tmp), &hp, &herr);
#endif
    if (hp == NULL)
    {
        msg.perror(E_HOST, "Unbekannter Mailserver <%s>", server.c_str());
        close( sock);
        sock = -1;
        return;
    }

    memcpy(&s_in.sin_addr, hp->h_addr, hp->h_length);
    s_in.sin_port = htons(port);

    if (::connect(sock, (sockaddr*) &s_in, sizeof(struct sockaddr_in)) < 0)
    {
        msg.perror(E_CONNECT, "kann mich nicht mit Mailserver <%s> verbinden", (s[0] + ":" + s[1]).c_str());
        close( sock);
        sock = -1;
        return;
    }

#if defined(__MINGW32__) || defined(__CYGWIN__)
    unsigned long on = 1;
    ioctlsocket(sock, FIONBIO, &on);
#else
    int i;
    i = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, i | O_NONBLOCK);
#endif
    this->tag = "* ";
    read_answer();

    this->tag = "Mne ";
    snprintf(tmp, sizeof(tmp), ( this->tag + "login %s %s\r\n").c_str(), user.c_str(), passwd.c_str());
    write_cmd(tmp, strlen(tmp));

}

CsList Imap:: split(std::string str)
{
    std::string::size_type i;
    std::string c;
    int havespace = 0;
    CsList l;

    for ( i = 0; i<str.size(); ++i)
    {
        if ( str[i] == '"')  havespace = ( havespace ) ? 0 : 1;
        if ( str[i] == ' ' && havespace == 0 )
        {
            if ( c != "" ) l.add(c);
            c = "";
        }
        else
        {
            c.push_back(str[i]);
        }
    }
    if ( c != "" ) l.add(c);

    return l;
}
Imap::Folder Imap::getFolder()
{
    char tmp[10240];
    unsigned int i;
    Folder f;

    snprintf(tmp, sizeof(tmp), "%s\r\n", ( this->tag + "list \"\" \"*\"").c_str());
    if ( write_cmd(tmp, strlen(tmp)) == 0 )
    {

        for ( i = 0; answer.size() > 0 && i < (answer.size()-1); ++i)
        {
            CsList a = split(answer[i]);
            std::string str;
            std::string::size_type j;
            char c;

            if ( a.size() != 5 )
            {
                msg.perror(E_READ, "Unerwartete Anwort - breche Verbindung ab");
                msg.line("%s", answer[i].c_str());
                close( sock);
                sock = -1;
                return f;
            }
            str = a[4];
            c = a[3][1];
            j = 0;
            while ( (j = str.find(c, j) ) != std::string::npos )
            {
                str.replace(j, 1, "/");
                j += 1;
            }
            str = str.substr(1,str.size()-2);

            f.insert(Folder::value_type(a[4].substr(1,a[4].size()-2),str));
        }
    }

    return f;
}

void Imap::scan_bodystructure(std::string s, Header *h, std::string pre )
{
    std::string s1,s2,k;
    if ( s[0] != '(')
    {
        h->insert(Header::value_type(num, s));
        num = num + "i";
        return;
    }

    k = "1";
    s2 = s;
    while ( s2 != "" )
    {
        s = s2;
        s1 = getBracketvalue(s);
        scan_bodystructure(s1, h, pre + "i" + k);
        if ( s.length() > (s1.length() + 2) ) s2 = s.substr(s1.length() + 2); else s2 = "";
        k = k + "1";
    }
}

std::string Imap::mk_utf8(std::string str)
{
    std::string conv, value;
    std::string::size_type i,j,k;

    iconv_t iv;
    char *inbuf, *outbuf, *ci, *co;
    size_t innum,outnum;
    size_t iconv_value;
    int coded = 0;
    int base64 = 0;
    while ( 1 )
    {
        i = str.find("=?");
        k = str.find("?Q?", i + 2 );
        if ( k == std::string::npos )
        {
            base64 = 1;
            k = str.find("?B?");
        }

        j = str.find("?=",  k + 3 );

        if ( i == std::string::npos || j == std::string::npos || k == std::string::npos )
        {
            if ( ! coded )
            {
                ci = inbuf = (char *)str.c_str();
                innum = str.length();

                co = outbuf = new char[str.size() * 4];
                outnum = ( str.size() * 4 - 1);

                iv = iconv_open("utf-8", "iso-8859-1");
                iconv_value = iconv (iv, &ci, &innum, &co, &outnum);
                iconv_close(iv);

                *co = '\0';
                str = outbuf;
                delete[] outbuf;
            }

            return str;
        }

        coded = 1;
        conv  = str.substr(i+2, k - i - 2);
        value = str.substr(k+3, j - k - 3);

        str = str.substr(0,i) + str.substr(j + 2);

        if ( base64 )
        {
            CryptBase64 crypt;
            value = crypt.decode(value);
        }
        else
        {

            k = 0;
            while ( ( k = value.find("_",k)) != std::string::npos )
                value.replace(k,1," ");
            k = 0;
            while ( ( k = value.find("=",k)) != std::string::npos )
            {
                char cv[2];

                cv[0] = (char) strtol(value.substr(k + 1, 2).c_str(), NULL, 16);
                cv[1] = '\0';
                value.replace(k,3,cv);
            }
        }


        ci = inbuf = (char *)value.c_str();
        innum = value.length();

        co = outbuf = new char[value.size() * 4];
        outnum = ( value.size() * 4 - 1);

        iv = iconv_open("utf-8", conv.c_str());
        if ( iv != (iconv_t) -1 )
        {
            iconv_value = iconv (iv, &ci, &innum, &co, &outnum);
            iconv_close(iv);
            *co = '\0';
            str.insert(i, outbuf);
        }
        else
        {
            str.insert(i, value);
        }

        delete[] outbuf;
    }
}
Imap::Headers Imap::getHeader(std::string mbox, time_t t )
{
    char tmp[10240];
    char ts[1024];
    unsigned int i, j;
    std::string::size_type pos;
    Headers h;
    CsList line;
    CsList mails;
    std::string s;
#if defined(__MINGW32__) || defined(__CYGWIN__)
#else
    struct tm tm;
#endif

#if defined(__MINGW32__) || defined(__CYGWIN__)
    std::string loc;
    loc = setlocale(LC_NUMERIC, NULL);
    setlocale(LC_NUMERIC | LC_TIME , "C");
    strftime(ts, sizeof(ts), "%d-%b-%Y", gmtime(&t) );

    setlocale(LC_NUMERIC | LC_TIME, loc.c_str());
#else
    locale_t loc, old_loc;
    loc = newlocale(LC_TIME_MASK | LC_NUMERIC_MASK, "C", NULL);
    old_loc = uselocale(loc);

    strftime(ts, sizeof(ts), "%d-%b-%Y", gmtime_r(&t, &tm) );

    uselocale(old_loc);
    if ( loc != NULL ) freelocale(loc);
#endif

    snprintf(tmp, sizeof(tmp), ( this->tag + "EXAMINE \"%s\"\r\n").c_str(), mbox.c_str());
    if ( write_cmd(tmp, strlen(tmp)) < 0 ) return h;
    if ( answer.size() == 0 || answer[answer.size() - 1] == "" || strncmp(answer[answer.size() - 1].c_str(), (this->tag + "NO").c_str(), (this->tag + "NO").size()) == 0 ) return h;

    snprintf(tmp, sizeof(tmp), ( this->tag + "SEARCH SINCE \"%s\"\r\n").c_str(), ts);

    if ( write_cmd(tmp, strlen(tmp)) < 0 ) return h;
    mails.setString(answer[0], ' ');

    for ( i = 2; i<mails.size(); ++i )
    {
        snprintf(tmp, sizeof(tmp), ( this->tag + "FETCH %s:%s (UID BODY BODY.PEEK[HEADER.FIELDS (DATE FROM TO CC BCC MESSAGE-ID SUBJECT IN-REPLY-TO)])\r\n").c_str(), mails[i].c_str(), mails[i].c_str());
        if ( write_cmd(tmp, strlen(tmp)) < 0 ) return h;

        for ( j = 0; answer.size() > 0 && j < (answer.size()-1); ++j)
        {
            if ( answer[j][0] == '*')
            {
                Header mh;
                std::string key,value;

                pos = answer[j].find("(");
                s = answer[j].substr(pos +1);
                line.setString(s, ' ');
                if ( line[0] == "UID" )
                    mh.insert(Header::value_type("UID", line[1]));
                if ( line[2] == "BODY" )
                {
                    num = "Body ";
                    s = getBracketvalue(s);
                    mh.insert(Header::value_type("BODY", s));
                    scan_bodystructure(s, &mh, "");
                 }
                for (++j;  j < (answer.size()-1) && answer[j][0] != ')' && answer[j].find("BODY[TEXT]") == std::string::npos; ++j )
                {
                    pos = answer[j].find(":");
                    if ( pos != std::string::npos )
                    {
                        key = answer[j].substr(0,pos);
                        std::transform(key.begin(), key.end(), key.begin(), (int(*)(int)) toupper);
                        value = (( pos + 2 ) < answer[j].size() ) ? answer[j].substr(pos+2) : "";
                        value = mk_utf8(value);
                        mh.insert(Header::value_type(key, value));
                     }
                    else
                        mh[key] = mh[key] + answer[j];
                }
                sprintf(tmp,"%d",i);
                h.insert(Headers::value_type(tmp,mh));
            }
        }
    }
    return h;
}

std::string Imap::getText(std::string uid )
{
    char tmp[10240];
    CsList mails;
    std::string str;
    std::string::size_type i,j;

    snprintf(tmp, sizeof(tmp), ( this->tag + "SEARCH UID %s\r\n").c_str(), uid.c_str());
    if ( write_cmd(tmp, strlen(tmp)) < 0 ) return "";
    mails.setString(answer[0], ' ');

    snprintf(tmp, sizeof(tmp), ( this->tag + "FETCH %s:%s (BODY.PEEK[HEADER] BODY.PEEK[TEXT])\r\n").c_str(), mails[2].c_str(), mails[2].c_str());
    if ( write_cmd(tmp, strlen(tmp), 0) < 0 ) return "";

    str = response;
    if ( ( i = str.find("\r\n")) == std::string::npos )
    {
        msg.perror(E_READ, "Unerwartete Anwort Start");
        msg.line("%s", response );
        return "";
    }

    str = str.substr(i+2);
    if ( ( i = str.find("BODY[TEXT]")) == std::string::npos )
    {
        msg.perror(E_READ, "Unerwartete Anwort BODY[TEXT]");
        msg.line("%s", response );
        return "";
    }

    if ( ( j = str.find("\r\n",i)) == std::string::npos )
    {
        msg.perror(E_READ, "Unerwartete Anwort BODY[TEXT] Ende");
        msg.line("%s", response );
        return "";
    }
    str.replace(i, j - i + 1, "");

    if ( ( i = str.rfind(")\r\n" + this->tag + "OK Fetch completed")) == std::string::npos )
    {
        msg.perror(E_READ, "Unerwartete Anwort FETCH completed");
        msg.line("%s", response );
        return "";
    }

    str = str.substr(0,i);
    return str;

}
