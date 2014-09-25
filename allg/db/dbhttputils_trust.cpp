#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <argument/argument.h>
#include <utils/process.h>
#include <utils/process.h>

#include "dbconnect.h"
#include "dbhttputils_trust.h"

#if defined(__MINGW32__) || defined(__CYGWIN__)
int
inet_aton(const char *cp_arg, struct in_addr *addr)
{
    register const u_char *cp = (u_char*)cp_arg;
    register u_long val;
    register int base;
#ifdef WIN32
    register ULONG_PTR n;
#else
    register unsigned long n;
#endif
    register u_char c;
    u_int parts[4];
    register u_int *pp = parts;

    for (;;) {
        /*
         * Collect number up to ``.''.
         * Values are specified as for C:
         * 0x=hex, 0=octal, other=decimal.
         */
        val = 0; base = 10;
        if (*cp == '0') {
            if (*++cp == 'x' || *cp == 'X')
                base = 16, cp++;
            else
                base = 8;
        }
        while ((c = *cp) != '\0') {
            if (isascii(c) && isdigit(c)) {
                val = (val * base) + (c - '0');
                cp++;
                continue;
            }
            if (base == 16 && isascii(c) && isxdigit(c)) {
                val = (val << 4) +
                        (c + 10 - (islower(c) ? 'a' : 'A'));
                cp++;
                continue;
            }
            break;
        }
        if (*cp == '.') {
            /*
             * Internet format:
             * a.b.c.d
             * a.b.c (with c treated as 16-bits)
             * a.b (with b treated as 24 bits)
             */
            if (pp >= parts + 3 || val > 0xff)
                return (0);
            *pp++ = val, cp++;
        } else
            break;
    }
    /*
     * Check for trailing characters.
     */
    if (*cp && (!isascii(*cp) || !isspace(*cp)))
        return (0);
    /*
     * Concoct the address according to
     * the number of parts specified.
     */
    n = pp - parts + 1;
    switch (n) {

    case 1: /* a -- 32 bits */
        break;

    case 2: /* a.b -- 8.24 bits */
        if (val > 0xffffff)
            return (0);
        val |= parts[0] << 24;
        break;

    case 3: /* a.b.c -- 8.8.16 bits */
        if (val > 0xffff)
            return (0);
        val |= (parts[0] << 24) | (parts[1] << 16);
        break;

    case 4: /* a.b.c.d -- 8.8.8.8 bits */
        if (val > 0xff)
            return (0);
        val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
        break;
    }
    if (addr)
        addr->s_addr = htonl(val);
    return (1);
}
#endif
DbHttpUtilsTrust::DbHttpUtilsTrust(DbHttp *h)
:DbHttpProvider(h),
 msg("DbHttpUtilsTrust")
{
    this->nologin = 0;
    subprovider["checkuser.html"] = &DbHttpUtilsTrust::check_user;
    h->add_provider(this);
}

DbHttpUtilsTrust::~DbHttpUtilsTrust()
{
}

int DbHttpUtilsTrust::check_request(Database *db, HttpHeader *h)
{
    return 1;
}

int DbHttpUtilsTrust::request(Database *db, HttpHeader *h, int nologin )
{
    int result;

    this->nologin = 1;
    result = request(db, h);
    this->nologin = 0;

    return result;
}

int DbHttpUtilsTrust::request(Database *db, HttpHeader *h)
{
    std::string name;
    std::string::size_type n;

    SubProviderMap::iterator i;

    if ( (i = subprovider.find(h->filename)) != subprovider.end() )
    {
        h->status = 200;
        (this->*(i->second))(db, h);

        if (h->vars["sqlend"] != "")
            db->p_getConnect()->end();

        return 1;
    }

    n = h->filename.find_last_of('.');
    if ( n != std::string::npos )

        name = h->filename.substr(0, n);

    h->status = 200;
    this->execute(db, h, name);
    return 1;
}

int DbHttpUtilsTrust::check_ip(const char *ip, int host)
{
    struct in_addr taddr, caddr;
    unsigned long mask = -1;
    CsList ipaddr (ip, '/');

    if ( this->nologin == 0 || ipaddr.size() == 0 ) return 1;

    CsList addr(ipaddr[0],'.');
    if ( addr.size() != 4 )
    {
        msg.perror(E_PERM, "IP Addresse <%> in falschem Format", ip);
        return 0;
    }

    if ( ipaddr.size() == 2 )
        mask = htonl(mask << ( 32 - atoi(ipaddr[1].c_str()) ));

    caddr.s_addr = host;
    caddr.s_addr &= mask;

    inet_aton(ipaddr[0].c_str(), &taddr );
    taddr.s_addr &= mask;

    if ( caddr.s_addr == taddr.s_addr )
        return 1;

    return 0;
}


void DbHttpUtilsTrust::check_user(Database *db, HttpHeader *h)
{
    Argument a;
    Database *d = db->getDatabase();
    unsigned int i;
    int host;

    Argument::StringWerte ips;

    ips = (a["DbTrustCheckUserip"]).getStringWerte();

    host = this->http->getServersocket()->getHost(h->client);
    for ( i = 0; i < ips.size(); ++i )
        if (  check_ip(ips[i].c_str(), host ) ) break;

    d->p_getConnect("", h->user, h->passwd);

    if ( i != ips.size() && d->have_connection() )
        h->status = 200;
    else
        h->status = 401;

    delete d;

}
void DbHttpUtilsTrust::execute(Database *db, HttpHeader *h, std::string name)
{
    std::string stm;
    char buffer[1024];

    CsList cols;
    DbTable::ValueMap where;
    DbConnect::ResultMat* result;
    DbConnect::ResultMat::iterator ri;
    int host;

    DbTable *tab = db->p_getTable("mne_application", "trustrequest");

    cols.add("action");
    cols.add("ipaddr");
    cols.add("typ");
    cols.add("validpar");

    where["name"] = name;

    result = tab->select(&cols, &where);
    tab->end();

    host = this->http->getServersocket()->getHost(h->client);
    for ( ri = result->begin(); ri != result->end(); ++ri )
    {
        if (  check_ip(((std::string) (*ri)[1]).c_str(), host ) ) break;
    }

    if ( ri == result->end() )
    {
        h->content_type = "text/plain";
        h->status = 404;
        rewind(h->content);
        fprintf(h->content, "error");
        msg.perror(E_NOFUNC, "keine Funktion für den Namen <%s> gefunden", name.c_str());
        return;
    }

    if ( (std::string) (*ri)[2] == "sql" )
    {
        stm = (std::string) (*ri)[0];

        if ( db->p_getConnect()->execute(stm.c_str()) == 0 )
        {
            DbConnect::ResultMat *r = db->p_getConnect()->p_getResult();
            if ( r->size() == 0 )
                fprintf(h->content,"ok");
            else
                fprintf(h->content,"%s",ToString::mkhtml(((*r)[0][0]).format(&msg)).c_str());
        }
        else
        {
            fprintf(h->content,"error");
        }

        db->p_getConnect()->end();
    }
    else if ( (std::string) (*ri)[2] == "shell" )
    {
        Process p(DbHttpProvider::http->getServersocket());
        CsList cmd;
        CsList validpar((char *)((*ri)[3]));
        int anzahl;
        HttpVars::Vars::iterator i;
        std::map<std::string,std::string>::iterator m;
        Argument a;

        cmd.add((std::string) (*ri)[0]);

        DbHttpAnalyse::Client::Userprefs userprefs = this->http->getUserprefs();
        DbHttpAnalyse::Client::Userprefs::iterator ui;
        for ( ui = userprefs.begin(); ui != userprefs.end(); ++ui)
        {
            cmd.add("-" + ui->first);
            cmd.add(ui->second);
        }

        for (m = h->datapath.begin(); m != h->datapath.end(); ++m )
        {
            cmd.add("-datapath");
            cmd.add(m->first);
            cmd.add(ToString::substitute(ToString::substitute(m->second.c_str(), "\\", "/"), "C:", "/cygdrive/c"));
        }

        cmd.add("-db");
        cmd.add(a["DbDatabase"]);

        cmd.add("-content_type");
        cmd.add(h->content_type);

        cmd.add("-hostname");
        cmd.add(h->hostname);

        cmd.add("-port");
        cmd.add(h->port);

        for ( i= h->vars.p_getVars()->begin(); i != h->vars.p_getVars()->end(); ++i )
        {
            if ( validpar.empty() || validpar.find(i->first) != std::string::npos )
            {
                cmd.add("-" + i->first);
                cmd.add(i->second);
            }
            else
            {
                h->content_type = "text/plain";
                h->status = 404;
                rewind(h->content);
                fprintf(h->content, "error");
                msg.perror(E_NOFUNC, "keine Funktion für den Namen <%s> gefunden", name.c_str());
                return;
            }
        }

#if defined(__MINGW32__) || defined(__CYGWIN__)
        std::string str = "bash -c '";
        unsigned int j;
        for ( j = 0; j<cmd.size(); j++)
            str += " \"" + ToString::mascarade(cmd[j].c_str(), "\"") + "\"";
        str += "'";
        cmd.clear();
        cmd.add(str);
#endif
        p.start(cmd, "pipe", a["EmbedwebHttpMapsRoot"], NULL, NULL, 1);

        while( ( anzahl = p.read(buffer, sizeof(buffer))) != 0 )
        {
            if ( anzahl > 0 )
                fwrite(buffer, 1, anzahl, h->content);
            else if ( anzahl < 0 && errno != EAGAIN ) break;
        }

        if ( p.getStatus() != 0 )
        {
            h->content_type = "text/plain";
            snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", "error.txt");
            buffer[sizeof(buffer) -1] = '\0';
            h->extra_header.push_back(buffer);
            fprintf(h->content, "Fehler %d", p.getStatus());
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", h->vars.url_decode(h->filename.c_str()).c_str());
            buffer[sizeof(buffer) -1] = '\0';
            h->extra_header.push_back(buffer);
        }
    }
    else
    {
        h->content_type = "text/plain";
        snprintf(buffer, sizeof(buffer), "Content-Disposition: attachment; filename=\"%s\"", "error.txt");
        buffer[sizeof(buffer) -1] = '\0';
        h->extra_header.push_back(buffer);

        rewind(h->content);
        fprintf(h->content, "error");
        msg.perror(E_NOFUNC, "keine Funktion für den Namen <%s> gefunden", name.c_str());
        return;

    }
}

