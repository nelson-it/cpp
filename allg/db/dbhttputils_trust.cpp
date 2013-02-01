#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <utils/tostring.h>

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

    subprovider["function.html"]      = &DbHttpUtilsTrust::function_html;
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

    SubProviderMap::iterator i;

    if ( ( i = subprovider.find(h->filename)) != subprovider.end() )
    {
        if ( h->filename.find(".xml") == strlen(h->filename.c_str()) - 4 )
        {
            h->status = 200;
            h->content_type = "text/xml";
            fprintf(h->content,
                    "<?xml version=\"1.0\" encoding=\"%s\"?><result>",
                    h->charset.c_str());
            (this->*(i->second))(db, h);
        }
        else
        {
            h->status = 200;
            (this->*(i->second))(db, h);
        }

        return 1;
    }

    return 0;

}

void DbHttpUtilsTrust::function_html(Database *db, HttpHeader *h)
{
    std::string stm;

    CsList cols;
    DbTable::ValueMap where;
    DbConnect::ResultMat* result;
    DbConnect::ResultMat::iterator ri;

    DbTable *tab = db->p_getTable("mne_application", "trustrequest");

    cols.add("action");
    cols.add("ipaddr");

    where["name"] = h->vars["name"];

    result = tab->select(&cols, &where);
    tab->end();

    for ( ri = result->begin(); ri != result->end(); ++ri )
    {
        struct in_addr taddr, caddr;
        unsigned long mask = -1;
        CsList ipaddr ((std::string) (*ri)[1], '/');

        if ( this->nologin == 0 || ipaddr.size() == 0 ) break;

        CsList addr(ipaddr[0],'.');
        if ( addr.size() != 4 )
        {
            msg.perror(E_PERM, "IP Addresse <%> in falschem Format", (char *)(*ri)[1]);
            continue;
        }

        if ( ipaddr.size() == 2 )
            mask = htonl(mask << ( 32 - atoi(ipaddr[1].c_str()) ));

        caddr.s_addr = this->http->getServersocket()->getHost(h->client);
        caddr.s_addr &= mask;

        inet_aton(ipaddr[0].c_str(), &taddr );
        taddr.s_addr &= mask;

        if ( caddr.s_addr == taddr.s_addr )
            break;
    }

    if ( ri == result->end() )
    {
        fprintf(h->content, "error");
        msg.perror(E_NOFUNC, "keine Funktion für den Namen <%s> gefunden", h->vars["name"].c_str());
        return;
    }

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

