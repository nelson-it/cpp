#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <mbctype.h>
#endif

#include "ws_analyse.h"
#include "ws.h"

void DumpHex(const void* data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char*)data)[i]);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i+1) % 8 == 0 || i+1 == size) {
            printf(" ");
            if ((i+1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i+1 == size) {
                ascii[(i+1) % 16] = '\0';
                if ((i+1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
            }
        }
    }
}

void *Thread(void *param)
{

#if defined(__MINGW32__) || defined(__CYGWIN__)
    _setmbcp(_MB_CP_ANSI);
#endif

    WsAnalyse::ThreadParam *p;

    p = (WsAnalyse::ThreadParam*)param;
    p->ws->init_thread();

    while(1)
    {
        pthread_mutex_lock(&p->mutex);
        if ( p->abort ) break;

        p->act_c = p->analyse->getClient();
        p->ws->get(p->act_c);
        p->analyse->releaseClient(p->act_c);
        p->act_c = NULL;
        pthread_mutex_unlock(&p->mutex);
    }

    pthread_mutex_unlock(&p->mutex);
    pthread_exit(NULL);
    return NULL;
}

WsAnalyse::ThreadParam::ThreadParam(Ws *ws, WsAnalyse *a)
{
    this->ws = ws;
    this->analyse = a;
    this->abort = 0;
    this->act_c = NULL;

    pthread_mutex_init(&this->mutex,NULL);
    pthread_create(&(this->id), NULL, Thread, (void *)this);
}

WsAnalyse::ThreadParam::~ThreadParam()
{
    pthread_mutex_lock(&mutex);
    this->abort = 1;
    pthread_mutex_unlock(&mutex);
}

void WsAnalyse::ThreadParam::disconnect(int client)
{
    this->ws->disconnect(client);
}

unsigned long WsAnalyse::Client::request(unsigned char *buffer, unsigned long size)
{
    unsigned long rest = size;
    while ( need_data <= rest && state != R_READY )
    {
        if ( need_data > max_length)
        {
            unsigned char *tmp = new unsigned char[need_data];
            delete [] data;
            data = tmp;
        }
        memcpy(data, &buffer[size - rest], need_data );

        rest -= need_data;
        if ( state == R_START )
        {
            opcode = data[0] & 0x0F;
            fin = (data[0] >> 7) & 0x01;
            masked = (data[1] >> 7) & 0x01;

            length = data[1] & (~0x80);
            switch(length)
            {
            case 126:
                need_data = 2;
                state = R_LENGTH2;
                break;
            case 127:
                need_data = 8;
                state = R_LENGTH3;
                break;
            default:
                if ( masked )
                {
                    need_data = 4;
                    state = R_MASK;
                }
                else
                {
                    need_data = length;
                    state = R_DATA;
                }
            }
        }
        else if ( state == R_LENGTH2 )
        {
            length = ( (data[0] << 8) | (data[1] ));
            if ( masked )
            {
                need_data = 4;
                state = R_MASK;
            }
            else
            {
                need_data = length;
                state = R_DATA;
            }

        }
        else if ( state == R_LENGTH3 )
        {
            length = (((unsigned long long int )data[0]) << 56) |
                    (((unsigned long long int )data[1]) << 48) |
                    (((unsigned long long int )data[2]) << 40) |
                    (((unsigned long long int )data[3]) << 32) |
                    (((unsigned long long int )data[4]) << 24) |
                    (((unsigned long long int )data[5]) << 16) |
                    (((unsigned long long int )data[6]) << 8) |
                    (((unsigned long long int )data[7]));

            if ( masked )
            {
                need_data = 4;
                state = R_MASK;
            }
            else
            {
                need_data = length;
                state = R_DATA;
            }
        }
        else if ( state == R_MASK )
        {
            memcpy( mask, data, 4);
            need_data = length;
            state = R_DATA;
        }
        else if ( state == R_DATA )
        {
            for(int i=0; i<length; i++)
                data[i] = data[i] ^ ((unsigned char*)(&mask))[i%4];
            data[length] = '\0';
            need_data = 0;
            state = R_READY;
        }
    }
    return rest;
}
WsAnalyse::WsAnalyse(ServerSocket *s)
:SocketProvider(s),
 msg("WsAnalyse")
{
    this->serversocket = s;

    pthread_mutex_init(&this->client_mutex,NULL);
    pthread_cond_init(&this->client_cond,NULL);

    s->add_provider(this);
}

WsAnalyse::~WsAnalyse()
{
    Clients::iterator i;
    for ( i = clients.begin(); i != clients.end(); i++) delete i->second;
}

void WsAnalyse::request( int client, char *buffer, long size )
{
    Client *c;
    Clients::iterator i;
    unsigned int rest = size;

    DumpHex(buffer, size);

    if ( ( i = clients.find(client) ) != clients.end() )
        c = i->second;
    else
        clients[client] = c = new Client(client);

    while ( rest != 0 )
    {
        rest = c->request((unsigned char*) &buffer[size - rest], rest);

        if ( c->state == Client::R_READY )
        {
            putClient(c);
            c = clients[client];
        }
    }
}

void WsAnalyse::disconnect( int client)
{
        Clients::iterator c;
        ClientQueue::iterator q;

        unsigned int i;

        if ( ( c = clients.find(client)) != clients.end() )
        {
            msg.pdebug( D_CON, "Verbindung zum Client %d wurde abgebrochen",client);
            clients.erase(c);
        }
        else
        {
            msg.pdebug(D_CON, "Client %d existiert nicht",client);
        }

        pthread_mutex_lock(&client_mutex);
        q = queue.begin();
        while ( q != queue.end() )
        {
            if ( (*q)->client == client )
            {
               queue.erase(q);
               q = queue.begin();
            }
            else
            {
                ++q;
            }
        }
        pthread_mutex_unlock(&client_mutex);

        for ( i = 0; i<wss.size(); ++i)
            wss[i]->disconnect(client);

}

void WsAnalyse::add_ws(Ws *ws)
{
    wss.push_back(new ThreadParam(ws, this));
}

void WsAnalyse::del_ws(Ws *ws)
{
    msg.pwarning(W_WS, "Ws kann nicht gelöscht werden");
    return;
}


