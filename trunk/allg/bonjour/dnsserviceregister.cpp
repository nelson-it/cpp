#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#ifdef MACOS

#include <dns_sd.h>

#include <message/message.h>

static void register_callback(DNSServiceRef service, DNSServiceFlags flags,
        DNSServiceErrorType errorCode, const char * name, const char * type,
        const char * domain, void * context)
{
    Message msg("DnsServiceRegister");
#pragma unused(flags)
#pragma unused(context)

    if (errorCode != kDNSServiceErr_NoError) msg.perror(1,
            "Fehler bei der Registrierung bei Bonjour %d\n", errorCode);
}

void register_bonjour(DNSServiceRef * ssr, uint16_t port, const char *name)
{
    Message msg("DnsServiceRegister");

    int err = 0;

    DNSServiceFlags flags = kDNSServiceFlagsDefault;
    uint32_t int_index = 0;
    const char *regtype = "_http._tcp";
    const char *domain = NULL; // default domain
    const char *host = NULL; // default host

    msg.pdebug(1, "Register service %s port: %d", regtype, port );
    err = DNSServiceRegister(ssr, flags, int_index, name, regtype, domain,
            host, htons(port), 0, NULL, register_callback, NULL);

    if (err)
    {
        msg.perror(1, "Fehler bei der Registrierung bei Bonjour %d\n", err);
    }
    return;

}
#endif
