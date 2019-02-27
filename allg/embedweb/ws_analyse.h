#ifndef ws_analyse_mne
#define ws_analyse_mne

#include <pthread.h>
#include <string>
#include <vector>
#include <deque>
#include <map>

#include <message/message.h>
#include <ipc/s_provider.h>

#include "http_header.h"

class Ws;

class WsAnalyse : public SocketProvider
{
public:
    class Client
    {
        enum ReadState
        {
            R_START,
            R_LENGTH2,
            R_LENGTH3,
            R_MASK,
            R_DATA,
            R_READY
        };

        friend class WsAnalyse;

        int client;
        int wait;

        unsigned char *data;
        unsigned char *end_data;
        int length = 0;
        unsigned int max_length = 1024;

        unsigned int need_data = 2;

        int frametype = 0;

        unsigned char opcode = 0;
        unsigned char fin = 0;
        unsigned char masked = 0;
        unsigned char mask[4] = { 0, 0, 0, 0 };

        int state = R_START;

    public:
        enum WebSocketFrameType
        {
            TEXT_FRAME=0x01,
            BINARY_FRAME=0x02,

            CLOSE_FRAME=0x08,
            PING_FRAME=0x09,
            PONG_FRAME=0x0A
        };

        unsigned char *act_data;

        Client(int client, int wait = 0 )
        {
            this->client = client;
            this->wait = wait;
            length = 0;
            end_data = act_data = data = new unsigned char[max_length];
        }
        ~Client()
        {
            delete[] data;
        }

        unsigned long request(unsigned char *buffer, unsigned long size);
        unsigned char *p_getData() { return act_data; }
        int getLength() { return ( end_data - act_data ); }
        int getClient() { return client; }
        int getOpcode() { return opcode; }

    };

    class ThreadParam
    {
    public:
        pthread_t id;
        pthread_mutex_t mutex;

        Ws *ws;
        WsAnalyse *analyse;

        WsAnalyse::Client *act_c;
        int abort;

        ThreadParam(Ws *ws, WsAnalyse *a);
        ~ThreadParam();

        void disconnect(int client);
    };

private:

    pthread_mutex_t client_mutex;
    pthread_cond_t  client_cond;

protected:
    typedef std::map<int, Client*> Clients;
    Clients clients;

    typedef std::deque<Client *> ClientQueue;
    ClientQueue queue;

    typedef std::vector<ThreadParam *> Wss;
    Wss wss;

public:
    void freenext(WsAnalyse::Client *c)
    {
        ClientQueue::iterator q;
        Clients::iterator i;

        pthread_mutex_lock(&client_mutex);
        for ( q = queue.begin(); q != queue.end(); ++q )
        {
            if ( (*q)->client == c->client )
            {
                (*q)->wait = 0;
                pthread_cond_signal(&client_cond);
                break;
            }
        }
        if ( q == queue.end() &&  ( i = clients.find(c->client)) != clients.end() )
        {
            i->second->wait = 0;
            pthread_cond_signal(&client_cond);
        }
        pthread_mutex_unlock(&client_mutex);
    }

    void putClient(WsAnalyse::Client *c)
    {
        pthread_mutex_lock(&client_mutex);
        c->end_data = c->data + c->length;
        if ( c->opcode != Client::CLOSE_FRAME ) queue.push_back(c); else delete c;
        clients[c->client] = new Client(c->client, 1);
        pthread_cond_signal(&client_cond);
        pthread_mutex_unlock(&client_mutex);
    }

    WsAnalyse::Client *getClient()
    {
        WsAnalyse::Client *ret = NULL;
        ClientQueue::iterator i;
        int mustwait = 0;

        pthread_mutex_lock(&client_mutex);
        while ( 1 )
        {
            if ( queue.empty() || mustwait )
                pthread_cond_wait(&client_cond, &client_mutex);

            mustwait = 1;
            for ( i = queue.begin(); i != queue.end(); ++i )
                if ( (*i)->wait != 1 )
                {
                    ret = (*i);
                    queue.erase(i);
                    pthread_mutex_unlock(&client_mutex);
                    return ret;
                }
        }
    }

    void releaseClient(WsAnalyse::Client *c)
    {
        freenext(c);
        delete c;
    }

protected:
    Message msg;
    ServerSocket *serversocket;

    enum ERROR_TYPE
    {
        MAX_ERROR = 100
    };

    enum WARNING_TYPE
    {
        W_WS = 1,
        MAX_WARNING = 100
    };

    enum DEBUG_TYPE
    {
        D_CON  = 1,
        D_SEND = 2,
        D_HEADER = 4,
        D_RAWHEADER = 4,
        D_HTTP = 5,
    };

public:
    WsAnalyse( ServerSocket *s);
    virtual ~WsAnalyse();

    std::string getProvidername() { return "Ws";  }
    void request( int client, char *buffer, long size );

    virtual void disconnect( int client );

    void add_ws( Ws *ws);
    void del_ws( Ws *ws);


};

#endif /* ws_analyse_mne */
