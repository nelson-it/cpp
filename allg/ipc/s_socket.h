#ifndef s_socket_mne
#define s_socket_mne

#if ! defined(__MINGW32__) && ! defined(__CYGWIN__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include<winsock2.h>
#include<ws2tcpip.h>
extern int      inet_aton(const char *cp, struct in_addr * addr);
#endif


#include <sys/time.h>

#include <map>
#include <set>

#include <message/message.h>

#include <pthread.h>

class SocketProvider;
class TimeoutClient;

class ServerSocket
{
protected:

    pthread_mutex_t mutex;
    pthread_mutex_t timeout_mutex;

    friend class TimeoutClient;

    enum ERROR_TYPES
    {
        OK = 0,
        E_SOCK_OPEN,
        E_SOCK_BIND,
        E_SELECT,
        E_ACCEPT,
        E_CLIENT_READ,
        E_PRO_EXISTS,
        E_PRO_NOT_EXISTS,
        E_PRO_UNKNOWN,
        E_HTTP_NULL,
        E_NO_CLIENT,
        E_WRITE,
        E_SOCKNUM,

        E_MAXERROR = 100

    };

    enum DEBUG_TYPES
    {
        D_CON   = 3,
        D_PROV  = 5,
        D_TIME  = 7,
        D_RD    = 10
    };

    // class Client zum Verwalten eines Clients in einer Map
    // =====================================================
    class Client
    {
        friend class ServerSocket;

        ServerSocket *s;
        SocketProvider *p;

        int fd;

        char *buffer;
        int index;
        int length;

        std::string host;
        struct sockaddr_storage sin;


        int need_close;

        void write(char *buffer, int lenght);
        void write(FILE *fp, int lenght);
        void write();
        void write_all();

        int empty() { return buffer == NULL; }

    public:

        Client();
        Client( ServerSocket *s, SocketProvider *p, int fd, struct sockaddr_storage *sin, int addrlen );
        Client( const Client &in);
        Client &operator=( const Client &in);

        ~Client();

        std::string getHost() { return host; }

        struct sockaddr_storage *getAddr() { return &sin; }

        void setAddr(std::string host);
        void setProvider(std::string name)
        {
            this->p = this->s->get_provider(name);
            if ( p == NULL) need_close = 1;
        }

    };

    // class TimeSort sortiert die Timeouts richtig
    // ============================================
    class TimeSort
    {
    public:
        bool operator()
        ( timeval const &t1, timeval const &t2 ) const;
    };

    friend class Client;

    // Member Variablen
    // ================
    Message msg;
    int sock;
    short socketnum;
    SocketProvider *http;

    fd_set *rd_set;
    fd_set *wr_set;
    int max_sock;

    // Verwalten der Provider
    // ++++++++++++++++++++++
    std::map<std::string, SocketProvider*> provider;

    // Verwalten der Clients
    // +++++++++++++++++++++
    typedef std::map<int, Client> Clients;
    Clients clients;

    // Verwalten der TimeoutClients
    // ++++++++++++++++++++++++++++
    typedef std::multimap<timeval, TimeoutClient *, TimeSort> Timeouts;
    Timeouts timeout_clients;

public:
    ServerSocket(short socketnum );
    ~ServerSocket();

    void add_provider( SocketProvider *p);
    void del_provider( SocketProvider *p);
    SocketProvider *get_provider( std::string name)
    {
        std::map<std::string, SocketProvider*>::iterator i;
        if ( ( i = provider.find(name)) != provider.end() ) return i->second;
        msg.perror(E_PRO_NOT_EXISTS, "Provider <%s> nicht gefunden", name.c_str());
        return NULL;
    }

    void add_timeout( TimeoutClient *t);
    void del_timeout( TimeoutClient *t);

    int accept(struct sockaddr_storage &c);
    void write( int client, char *buffer, int size);
    void write( int client, FILE *fp,     int size);
    void flush( int client );
    int  read ( int client, char *buffer, int size);
    void close( int client);

    std::string getHost(int client ) { return clients[client].getHost(); }
    int getSocket() { return sock; }
    int getSocketnum() { return (unsigned short)socketnum; }

    struct sockaddr_storage *getAddr(int client) { return clients[client].getAddr(); }
    void setAddr(int client, std::string host) { clients[client].setAddr(host); }
    void setProvider(int client, std::string name ) { clients[client].setProvider(name); }

    void loop();
};

#endif /* s_socket_mne */

