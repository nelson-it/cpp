#ifndef dnsserviceregister_mne
#define dnsserviceregister_mne

#ifdef MACOS
#include <dns_sd.h>

void register_bonjour(DNSServiceRef * ssr, uint16_t port, const char *name);
#endif

#endif /* dnsserviceregister_mne */
