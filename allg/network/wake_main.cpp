#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <winsock2.h>
#include <windows.h>
#endif

#include <utils/cslist.h>
#include <network/wake.h>
#include <argument/argument.h>

int main(int argc, char *argv[])
{
#if defined(__MINGW32__) || defined(__CYGWIN__)
    WSADATA wsa;
    if ( WSAStartup(MAKEWORD(2,0), &wsa) )
    {
        fprintf(stderr, "winsock start gescheitert\n");
        exit(1);
    }
#endif

    Argument::ListeMap liste;
    liste["whost"]  = Argument::Liste("-wh", 'c', -100, "");
    liste["wether"] = Argument::Liste("-we", 'c', -100, "");

    Argument a(&liste, *argv);
    a.scan(--argc, ++argv);

    Argument::StringWerte host  = a["whost"];
    Argument::StringWerte ether = a["wether"];

    unsigned int i;

    for ( i=0; i<host.size() && host[i] != ""; i++)
        Wake w(host[i].c_str(), ether[i].c_str());

#if defined(__MINGW32__) || defined(__CYGWIN__)
    WSACleanup();
#endif
}
