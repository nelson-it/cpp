#ifndef ws_provider_mne
#define ws_provider_mne

#include <string>

#include "ws_analyse.h"
#include "ws.h"

class WsProvider
{
protected:

    Ws *ws;
    WsProvider()
	{ this->ws = NULL; }
    
public:
     WsProvider( Ws *ws ) { this->ws = ws; }
     virtual ~WsProvider() { if ( ws != NULL ) ws->del_provider(this); }

     virtual std::string getOpcode() = 0;
     virtual int request (WsAnalyse::Client *c) = 0;
     virtual unsigned char *p_getData() = 0;
     virtual int getLength() = 0;

     virtual void disconnect (int client) {};
     virtual void init_thread() {};
};

#endif /* ws_provider_mne */
