#ifdef PTHREAD
#include <pthread.h>
#endif

#if defined(__CYGWIN__) || defined(__MINGW32__)
#include <winsock2.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <string>
#if defined(__MINGW32__) || defined(__CYGWIN__)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include "wake.h"

#include <errno.h>

Wake::Wake(const char *name, const char *e)
:msg("Wake")
{
	std::string ether(e);

	int i,j,err;

	int    sock;
	struct sockaddr_in addr;
	struct hostent *host;
#if defined(__MINGW32__) || defined(__CYGWIN__)
	unsigned char   buffer[256];
#else
	unsigned char   buffer[256];
#endif
	char   mac[6];
	int   optval = 1;


	if ( ether.find_first_of(":-") != std::string::npos && ether.size() == 17 )
	{
		mac[0] = strtol(ether.substr(0,2).c_str(),  NULL, 16);
		mac[1] = strtol(ether.substr(3,2).c_str(),  NULL, 16);
		mac[2] = strtol(ether.substr(6,2).c_str(),  NULL, 16);
		mac[3] = strtol(ether.substr(9,2).c_str(),  NULL, 16);
		mac[4] = strtol(ether.substr(12,2).c_str(), NULL, 16);
		mac[5] = strtol(ether.substr(15,2).c_str(), NULL, 16);
	}
	else if ( ether.size() == 12 )
	{
		mac[0] = strtol(ether.substr(0,2).c_str(),  NULL, 16);
		mac[1] = strtol(ether.substr(2,2).c_str(),  NULL, 16);
		mac[2] = strtol(ether.substr(4,2).c_str(),  NULL, 16);
		mac[3] = strtol(ether.substr(6,2).c_str(),  NULL, 16);
		mac[4] = strtol(ether.substr(8,2).c_str(),  NULL, 16);
		mac[5] = strtol(ether.substr(10,2).c_str(), NULL, 16);
	}
	else
	{
		msg.perror(E_ETHER, "Ethernetaddresse ist nicht korrekt formatiert");
		return;
	}

	if (( host = gethostbyname(name)) == NULL)
	{
		msg.perror(E_HOST, "Kann Host <%s> nicht auflösen", name);
		return;
	}

	addr.sin_family      = AF_INET;
	addr.sin_port        = htons(80);
	memcpy((char *) &addr.sin_addr, (char *) host->h_addr, host->h_length);

	for(i = 0; i<6; ++i)
		buffer[i] = 0xff;

	for(i = 1; i<17; ++i)
	{
		for(j = 0; j<6; ++j)
			buffer[(i)*6+(j)] = mac[j];
	}

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
	{
		msg.perror(E_SOCKET, "Kann Socket nicht erzeugen");
	}

	if ( sock >= 0 && (err = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&optval, sizeof(optval))) < 0 )
	{
		msg.perror(E_BROADCAST, "Kann Socketoption <SO_BROADCAST> nicht setzen");
		msg.line("errrorcode: %d", errno);
	}

	if ( sock >=0 && sendto (sock, (char *)buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, sizeof (addr)) < 0)
		msg.perror(E_SEND, "Kann Wakeuptelegramm zu <%s:%s> nicht senden", name, e);

	host->h_addr[3] = -1;
	memcpy((char *) &addr.sin_addr, (char *) host->h_addr, host->h_length);
	if ( sock >=0 && sendto (sock, (char *)buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, sizeof (addr)) < 0)
		msg.perror(E_SEND, "Kann Wakeuptelegramm zu <%s:%s> nicht senden", name, e);

	host->h_addr[2] = -1;
	memcpy((char *) &addr.sin_addr, (char *) host->h_addr, host->h_length);
	if ( sock >=0 && sendto (sock, (char *)buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, sizeof (addr)) < 0)
		msg.perror(E_SEND, "Kann Wakeuptelegramm zu <%s:%s> nicht senden", name, e);

	host->h_addr[1] = -1;
	memcpy((char *) &addr.sin_addr, (char *) host->h_addr, host->h_length);
	if ( sock >=0 && sendto (sock, (char *)buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, sizeof (addr)) < 0)
		msg.perror(E_SEND, "Kann Wakeuptelegramm zu <%s:%s> nicht senden", name, e);

	host->h_addr[0] = -1;
	memcpy((char *) &addr.sin_addr, (char *) host->h_addr, host->h_length);
	if ( sock >=0 && sendto (sock, (char *)buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, sizeof (addr)) < 0)
		msg.perror(E_SEND, "Kann Wakeuptelegramm zu <%s:%s> nicht senden", name, e);


    #if defined(__MINGW32__) || defined(__CYGWIN__)
	closesocket(sock);
#else
	close(sock);
#endif
}
