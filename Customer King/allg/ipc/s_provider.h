#ifndef provider_mne
#define provider_mne

#include <string>

#include "s_socket.h"

class SocketProvider
{
protected:

    ServerSocket *s;
public:
     SocketProvider( ServerSocket *s)
	 { this->s = s; }
     virtual ~SocketProvider()
	 { s->del_provider(this); }

     virtual std::string getProvidername() = 0;

     virtual void request ( int client, char *buffer, long size ) = 0;
     virtual void disconnect( int client ) {} ;
};

#endif /* provider_mne */
