#ifndef s_socket_mne
#define s_socket_mne

#if defined(__CYGWIN__) || defined(__MINGW32__)
#include <winsock.h>
#endif

#include <sys/time.h>
#include <map>
#include <set>

#include <message/message.h>

#ifdef PTHREAD
#include <pthread.h>
#endif

class SocketProvider;
class TimeoutClient;

class ServerSocket
{
    #ifdef PTHREAD
    pthread_mutex_t mutex;
    pthread_mutex_t timeout_mutex;
    #endif

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
	int fd;

        char *buffer;
	int index;
	int length;
        
        unsigned int host;
	unsigned short int port;

	int need_close;

	void write(char *buffer, int lenght);
	void write(FILE *fp, int lenght);
	void write();
	void write_all();
        
	int empty() { return buffer == NULL; }

    public:

        Client();
        Client( ServerSocket *s, int fd, struct sockaddr_in *sin);
	Client( const Client &in);
	Client &operator=( const Client &in);

	~Client();

	unsigned int getHost()       { return host; }
	unsigned short int getPort() { return port; }


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
    SocketProvider *http;

    fd_set *rd_set;
    fd_set *wr_set;
    int max_sock;

    // Verwalten der Provider
    // ++++++++++++++++++++++
    std::map<std::string, int> pnum;
    std::map<int, SocketProvider*> provider;
    int providerid;

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

    void add_timeout( TimeoutClient *t);
    void del_timeout( TimeoutClient *t);

    void write( int client, char *buffer, int size);
    void write( int client, FILE *fp,     int size);
    void flush( int client );
    int  read ( int client, char *buffer, int size);
    void close( int client);
    
    unsigned int getHost(int client )
        { return clients[client].getHost(); }
    unsigned short int getPort(int client)
        { return clients[client].getPort(); }

    void loop();
};

#endif /* s_socket_mne */

