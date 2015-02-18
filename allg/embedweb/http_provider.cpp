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

#include <utils/cslist.h>
#include "http_provider.h"

#if defined(__MINGW32__) || defined(__CYGWIN__)
int inet_aton(const char *cp_arg, struct in_addr *addr)
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

int HttpProvider::check_ip(const char *ip, int host)
{
    struct in_addr taddr, caddr;
    unsigned long mask = -1;
    CsList ipaddr (ip, '/');

    if ( ipaddr.size() == 0 ) return 1;

    CsList addr(ipaddr[0],'.');
    if ( addr.size() != 4 )
    {
        Message msg("HttpProvider");
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


