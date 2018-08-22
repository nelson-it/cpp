#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <sec_api/stdio_s.h>
#endif

#include <argument/argument.h>

#include <unistd.h>

#include "ws.h"
#include "ws_analyse.h"
#include "ws_provider.h"

Ws::Ws(ServerSocket *s, WsAnalyse *analyse, int register_thread ) :
    msg("Ws")
{

    Argument a;
    this->s = s;
    this->analyse = analyse;
    this->act_c = NULL;

    if (register_thread)
        analyse->add_ws(this);
}

Ws::~Ws()
{
    analyse->del_ws(this);
}

void Ws::send_frame(const unsigned char *content, int size)
{
    unsigned char *buffer = new unsigned char[size + 16];
    int pos = 0;

    buffer[pos++] = (unsigned char)0x81; // text frame

    if(size <= 125) {
        buffer[pos++] = size;
    }
    else if(size <= 65535) {
        buffer[pos++] = 0x7e; //16 bit length follows

        buffer[pos++] = (size >> 8) & 0xFF; // leftmost first
        buffer[pos++] = size & 0xFF;
    }
    else { // >2^16-1 (65535)
        buffer[pos++] = 0x7f; //64 bit length follows

        // write 8 bytes length (significant first)

        // since msg_length is int it can be no longer than 4 bytes = 2^32-1
        // padd zeroes for the first 4 bytes
        for(int i=3; i>=0; i--) {
            buffer[pos++] = 0;
        }
        // write the actual 32bit msg_length in the next 4 bytes
        for(int i=3; i>=0; i--) {
            buffer[pos++] = ((size >> 8*i) & 0xFF);
        }
    }

    memcpy((void*)(buffer+pos), content, size);
    this->s->write(act_c->getClient(), (char *)buffer,  pos + size);
    this->s->flush(act_c->getClient());

    delete[] buffer;
}

void Ws::read_opcode()
{
    unsigned char *c = act_c->p_getData();
    unsigned char *start = act_c->p_getData();
    unsigned char *end = act_c->p_getData() + act_c->getLength();
    std::string str;

    // Opcode
    // ======
    for ( start = c; c != end && *c != '\n' && *c != '\r'; ++c );
    act_opcode.assign((char*)start, c - start);
    while ( c != end && ( *c == '\n' || *c == '\r') ) ++c;

    act_c->act_data = c;
}

void Ws::get(WsAnalyse::Client *c)
{
    struct timeval t1, t2;
    long diff;
    WsProvider *p;

    gettimeofday(&t1, NULL);

    this->act_c = c;
    read_opcode();
    if (( p = find_provider(act_opcode)) != NULL )
    {
        if ( p->request(c) )
            send_frame( p->p_getData(), p->getLength());
    }


    gettimeofday(&t2, NULL);
    diff = (t2.tv_sec - t1.tv_sec ) * 1000000 + (t2.tv_usec - t1.tv_usec);
    msg.pdebug(1, "Ausführungszeit: %d usec", diff);

}

void Ws::disconnect(int client)
{
    Provider::iterator i;

    for ( i = provider.begin(); i != provider.end(); ++i)
        i->second->disconnect(client);

}

WsProvider * Ws::find_provider(std::string opcode)
{
    Provider::iterator i = provider.find(opcode);
    if ( i != provider.end() ) return i->second;

    return NULL;
}

void Ws::add_provider(WsProvider *p)
{
    std::string opcode;
    opcode = p->getOpcode();

    if (provider.find(opcode) == provider.end() )
        provider[opcode] = p;
    else
        msg.perror(E_PRO_EXISTS, "WsProvider \"%s\" ist schon registriert", opcode.c_str());
}

void Ws::del_provider(WsProvider *p)
{
    std::string opcode;
    opcode = p->getOpcode();
    Provider::iterator i;

    if ( (i = provider.find(opcode)) != provider.end() )
        provider.erase(i);
    else
        msg.perror(E_PRO_NOT_EXISTS, "WsProvider \"%s\" ist nicht registriert", opcode.c_str());
}
