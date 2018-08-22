#include "fdsize.h"

#if defined(__CYGWIN__) || defined(__MINGW32__)
#include <winsock2.h>
#define SOCK_CLOEXEC 0
#endif

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#if defined (Darwin)
#define SOCK_CLOEXEC 0
#endif

#if defined (Darwin) || defined(LINUX)
#include <unistd.h>
#include <sys/stat.h>
#endif

#if 1
#define Pthread_mutex_lock(x,y) pthread_mutex_lock(y);
#define Pthread_mutex_unlock(x,y) pthread_mutex_unlock(y);
#else
#define Pthread_mutex_lock(x,y) fprintf(stderr, "lock %s %x\n", x, y);  \
        pthread_mutex_lock(y);
#define Pthread_mutex_unlock(x,y) fprintf(stderr, "unlock %s %x\n", x, y);  \
        pthread_mutex_unlock(y);
#endif

#if defined(__MINGW32__) || defined(__CYGWIN__)
#include <utils/gettimeofday.h>
#include <sys/stat.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include <string.h>

#include <vector>

#include "s_socket.h"
#include "s_provider.h"
#include "timeout.h"

ServerSocket::Client::Client()
{
    s = NULL;
    p = NULL;
    fd = -1;

    buffer = NULL;
    length = 0;
    index = 0;
    s = NULL;

    need_close = 0;
}

ServerSocket::Client::Client( ServerSocket *s, SocketProvider *p, int fd, struct sockaddr_storage *sin, int addrlen )
{
    char host[NI_MAXHOST];
    char port[NI_MAXSERV];

    this->s = s;
    this->fd = fd;
    this->p = p;

    buffer = NULL;
    length = 0;
    index = 0;

    *port = '\0';

    getnameinfo((struct sockaddr *)sin, addrlen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);

    if ( strncmp(host, "::ffff:", 7 ) == 0 )
    {
        struct sockaddr_in s;
        memset(&s, 0, sizeof(s));
        s.sin_family = AF_INET;
        s.sin_port = atoi(port);
        s.sin_addr.s_addr = *((unsigned long*)(((struct sockaddr_in6 *)sin)->sin6_addr.s6_addr + 12));
        sin = (struct sockaddr_storage *)&s;
        getnameinfo((struct sockaddr *)sin, addrlen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST);
    }

    this->host = host;
    this->port = port;
    memcpy(&this->sin, sin, addrlen);

    need_close = 0;
#if ! ( defined(__MINGW32__) || defined(__CYGWIN__) )
    int on;
    on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        s->msg.perror(E_SOCK_OPEN, "client konnte reuse Option nicht setzen");
        s->msg.line("%s", strerror(errno));
    }
#endif

}

ServerSocket::Client::Client( const Client &in)
{
    s = in.s;
    p = in.p;

    fd = in.fd;

    length = in.length;
    index = in.index;
    if ( length != 0 )
    {
        buffer = new char[length];
        memcpy(buffer, in.buffer, length);
    }
    else
    {
        buffer = NULL;
    }

    host = in.host;
    port = in.port;
    memcpy(&sin, &in.sin, sizeof(in.sin));

    need_close = in.need_close;
}

ServerSocket::Client &ServerSocket::Client::operator=( const Client &in)
{

    s = in.s;
    p = in.p;
    fd = in.fd;

    length = in.length;
    index = in.index;
    if ( length != 0 )
    {
        buffer = new char[length];
        memcpy(buffer, in.buffer, length);
    }
    else
    {
        buffer = NULL;
    }

    host = in.host;
    port = in.port;
    memcpy(&sin, &in.sin, sizeof(in.sin));

    need_close = in.need_close;

    return *this;
}

ServerSocket::Client::~Client()
{
    if ( buffer != NULL ) delete[] buffer;
}

void ServerSocket::Client::write(char *b, int l)
{

    if ( buffer != NULL )
    {
        char *tmp;
        tmp = new char[length - index + l];
        memcpy(tmp, &buffer[index], length - index);
        memcpy(&tmp[length - index], b, l);

        delete[] buffer;

        buffer = tmp;
        length = length - index + l;
        index = 0;
    }
    else
    {
        buffer = new char[l];
        memcpy(buffer, b, l);
        length = l;
        index = 0;
    }

}

void ServerSocket::Client::write(FILE *f, int l)
{

    s->msg.pdebug(ServerSocket::D_CON, "sende zu client %d: %d bytes", fd, l);

    if ( buffer != NULL )
    {
        char *tmp;
        tmp = new char[length - index + l];
        memcpy(tmp, &buffer[index], length - index);
        fread(&tmp[length - index], l, 1, f);

        delete[] buffer;

        buffer = tmp;
        length = length - index + l;
        index = 0;
    }
    else
    {

        buffer = new char[l];
        fread(buffer, l, 1, f);
        length = l;
        index = 0;
    }

}

void ServerSocket::Client::write()
{
    if ( buffer != NULL )
    {
        int result;

        s->msg.wdebug(D_RD, &buffer[index], length - index);
        result = ::send(fd, &buffer[index], length - index, 0);
        if ( result < 0 )
        {
            s->msg.perror(ServerSocket::E_WRITE, "Fehler beim Schreiben zum Client %d", fd);
#if defined(__MINGW32__) || defined(__CYGWIN__)
            s->msg.line("Fehlernummer %d", WSAGetLastError());
#else
            s->msg.line("%s", strerror(errno));
#endif
            need_close = 1;
            length = 0;
            index = 0;
        }
        else
        {
            index += result;
        }

        if ( index == length )
        {
            s->msg.pdebug(ServerSocket::D_CON, "daten zu client %d gesendet", fd);
            delete[] buffer;
            buffer = NULL;
            index = 0;
            length = 0;
        }
    }

}

void ServerSocket::Client::write_all()
{
    while( buffer != NULL )
        this->write();
}

void ServerSocket::Client::setAddr(std::string host, std::string port)
{
    int found = 0;

    if ( host != "-" && host != "" ) { this->host = host; found = 1; }
    if ( port != "-" && port != "" ) { this->port = port; found = 1; }

    if ( !found ) return;

    if ( this->host.find(':') != std::string::npos )
    {
        struct sockaddr_in6 s;
        memset(&s, 0, sizeof(s));
        s.sin6_family = AF_INET6;
        s.sin6_port = atoi(this->port.c_str());
        inet_pton(AF_INET6, this->host.c_str(), &(s.sin6_addr));
        memcpy(&this->sin, &s, sizeof(s));
    }
    else
    {
        struct sockaddr_in s;
        memset(&s, 0, sizeof(s));
        s.sin_family = AF_INET;
        s.sin_port = atoi(port.c_str());
        inet_aton(host.c_str(), &(s.sin_addr));
        memcpy(&this->sin, &s, sizeof(s));
    }


}

bool ServerSocket::TimeSort::operator()
             ( timeval const &tt1, timeval const &tt2 ) const
{
    timeval t1 = tt1;
    timeval t2 = tt2;

    if ( t2.tv_sec == 0 && t2.tv_usec == 0 )
    { t2.tv_sec = 0x7fffffff; t2.tv_usec = 0x7fffffff; }

    if ( t1.tv_sec == 0 && t1.tv_usec == 0 )
    { t1.tv_sec = 0x7fffffff; t1.tv_usec = 0x7fffffff; }

    return ( t1.tv_sec < t2.tv_sec ||
            ( t1.tv_sec == t2.tv_sec  && t1.tv_usec < t2.tv_usec ));
}

ServerSocket::ServerSocket(short socketnum )
:msg("ServerSocket")
{
    int length;
    int rval;
    struct sockaddr_in6 server;

    if ( ( sock = socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0 ) ) < 0 )
    {
        msg.perror(E_SOCK_OPEN, "konnte keinen socket für den Service öffnen");
        msg.line("%s", strerror(errno));
        exit(1);
    }

#if ! ( defined(__MINGW32__) || defined(__CYGWIN__) )
    int on;
    on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        msg.perror(E_SOCK_OPEN, "server konnte reuse Option nicht setzen");
        msg.line("%s", strerror(errno));
    }
#endif

    memset(&server, 0, sizeof(server));

    server.sin6_family = AF_INET6;
    server.sin6_flowinfo = 0;
    server.sin6_addr = in6addr_any;
    server.sin6_port = htons(socketnum);

    length = sizeof(server);

    if ( ( rval = bind(sock, (struct sockaddr *)&server, length)) < 0 )
    {
        msg.perror(E_SOCK_BIND,"konnte socket nich an port %d binden", socketnum);
        msg.line("%s", strerror(errno));
        exit(1);
    }

    http = NULL;
    wr_set = NULL;
    rd_set = NULL;
    max_sock = 0;

    pthread_mutex_init(&mutex,NULL);
    pthread_mutex_init(&timeout_mutex,NULL);

}

ServerSocket::~ServerSocket()
{
    ::close(sock);
}

void ServerSocket::add_provider( SocketProvider *p)
{
    std::string name;

    Pthread_mutex_lock("add provider", &mutex);

    name = p->getProvidername();

    if ( provider.find(name) == provider.end() )
    {
        msg.pdebug(D_PROV, "SocketProvider \"%s\" wird hinzugefügt", name.c_str());
        provider[name] = p;
    }
    else
    {
        msg.perror(E_PRO_EXISTS, "SocketProvider \"%s\" ist schon registriert", name.c_str());
    }

    Pthread_mutex_unlock("mutex", &mutex);
}

void ServerSocket::del_provider( SocketProvider *p)
{
    std::string name;

    Pthread_mutex_lock("del provider", &mutex);

    name = p->getProvidername();

    msg.pdebug(D_PROV, "SocketProvider \"%s\" wird entfernt", name.c_str());
    if ( provider.find(name) != provider.end() )
    {
        provider.erase(name);
    }
    else
    {
        msg.perror(E_PRO_NOT_EXISTS, "SocketProvider \"%s\" ist nicht registriert", name.c_str());
    }

    Pthread_mutex_unlock("mutex", &mutex);
}

void ServerSocket::add_timeout( TimeoutClient *t)
{
    Pthread_mutex_lock("add_timeout", &timeout_mutex);
    timeout_clients.insert(std::pair<timeval, TimeoutClient *>(*t,t));
    Pthread_mutex_unlock("timeout", &timeout_mutex);
}

void ServerSocket::del_timeout( TimeoutClient *t)
{
    Timeouts::iterator i;
    int n;

    Pthread_mutex_lock("del timeout", &timeout_mutex);
    i = timeout_clients.find(*(timeval *)t);
    for ( n = timeout_clients.count(*(timeval *)t); n > 0; ++i, --n )
    {
        if ( i->second == t )
        {
            timeout_clients.erase(i);
            break;
        }
    }
    Pthread_mutex_unlock("timeout",&timeout_mutex);
}

void ServerSocket::write(int client, char *buffer, int size)
{
    Clients::iterator i;

    Pthread_mutex_lock("write char", &mutex);
    if ( ( i = clients.find(client)) != clients.end() )
    {
        msg.pdebug(D_RD, "Schreibe zu Client %d %d bytes", client, size);

        i->second.write(buffer, size);
    }
    else
    {
        msg.pdebug(D_CON, "Client %d hat die Verbindung während des Schreibens beendet", client);
    }

    Pthread_mutex_unlock("mutex", &mutex);
}

void ServerSocket::write(int client, FILE *fp, int size)
{
    Clients::iterator i;

    Pthread_mutex_lock("write fp", &mutex);

    if ( ( i = clients.find(client)) != clients.end() )
    {
        msg.pdebug(D_RD, "Schreibe zu Client %d %d bytes", client, size);
        i->second.write(fp, size);
    }
    else
    {
        msg.pdebug(D_CON, "Client %d hat die Verbindung während des Schreibens beendet", client);
    }
    Pthread_mutex_unlock("mutex", &mutex);
}

void ServerSocket::flush( int client )
{
    Clients::iterator i;

    Pthread_mutex_lock("write fp", &mutex);
    if ( ( i = clients.find(client)) != clients.end() )
    {
        msg.pdebug(D_RD, "Flush Client %d", client);
        i->second.write_all();
        if ( i->second.need_close )
            ::close(i->first);
    }
    else
    {
        msg.pdebug(D_CON, "Client %d hat die Verbindung während des Schreibens beendet", client);
    }

    Pthread_mutex_unlock("mutex", &mutex);
}


int ServerSocket::read(int client, char *buffer, int size)
{
    int l, len;
    fd_set rd;

    msg.pdebug(D_CON, "Client %d liest noch %d werte", client, size );

    Pthread_mutex_lock("read", &mutex);
    FD_ZERO(&rd);
    for ( len = 0; len != size; )
    {

#if defined(__MINGW32__) || defined(__CYGWIN__)
        FD_SET((unsigned)client, &rd);
        select( client + 1, &rd, (fd_set*)0, (fd_set*)0, NULL);
        l = ::recv(client, &buffer[len], size - len, 0);
#else
        FD_SET(client, &rd);
        select( client + 1, &rd, (fd_set*)0, (fd_set*)0, NULL);
        l = ::read(client, &buffer[len], size - len);
#endif

        msg.pdebug(D_CON, "Client %d hat %d werte gelesen", client, l );
        if ( l > 0 )
            len += l;
        else if ( l == 0 )
        {
            Pthread_mutex_unlock("mutex", &mutex);
            msg.pdebug(D_CON, "Client %d hat die Verbindung während des Lesens beendet", client );
            return -1;
        }
        else
        {
            if ( errno != EINTR )
            {
                Pthread_mutex_unlock("mutex", &mutex);
                msg.pdebug(D_CON, "Client %d hat einen Fehler während des Lesens", client );
                msg.line("Grund %s", strerror(errno));
                return -1;
            }
        }
    }
    Pthread_mutex_unlock("mutex", &mutex);
    return size;
}

void ServerSocket::close(int client)
{
    Clients::iterator i;
    Pthread_mutex_lock("close", &mutex);

    if ( ( i = clients.find(client)) != clients.end() )
    {
        if ( ! i->second.empty() )
        {
            msg.pdebug(D_CON, "Anforderung zum Verbindungende zu Client %d", client);
            i->second.need_close = 1;
            Pthread_mutex_unlock("mutex",&mutex);
            return;
        }

        msg.pdebug(D_CON, "Verbindung zum Client %d wird beendet", client);

        std::map<std::string, SocketProvider*>::iterator ii;

        ::close(i->first);
        clients.erase(i);

        for ( ii = provider.begin(); ii != provider.end(); ++ii )
            ii->second->disconnect(i->first);

        FD_ZERO(rd_set);
#if defined(__MINGW32__) || defined(__CYGWIN__)
        FD_SET((unsigned)sock, rd_set);
#else
        FD_SET(sock, rd_set);
#endif
        max_sock = sock;
        for ( i=clients.begin(); i != clients.end(); ++i )
        {
#if defined(__MINGW32__) || defined(__CYGWIN__)
            FD_SET((unsigned)i->first, rd_set);
#else
            FD_SET(i->first, rd_set);
#endif
            if ( i->first > max_sock ) max_sock = i->first;
        }
    }
    else
    {
        msg.pdebug(D_CON, "Client %d existiert nicht", client);
    }
    Pthread_mutex_unlock("mutex",&mutex);
}

void ServerSocket::loop()
{
    Clients::iterator i;
    std::vector<int> del_clients;

    struct timeval act, to;
    fd_set rd_ready;
    fd_set wr_ready;

    Pthread_mutex_lock("loop", &mutex);

    // Umständliches Konstrukt um die Grösse verbergen zu können
    // =========================================================
    fd_set rd_set_local;
    fd_set wr_set_local;

    rd_set = &rd_set_local;
    wr_set = &wr_set_local;

    char buffer[10240];
    int rval,rsel;

    listen(sock, 5);

    FD_ZERO(rd_set);
    FD_ZERO(wr_set);

#if defined(__MINGW32__) || defined(__CYGWIN__)
    FD_SET((unsigned)sock, rd_set);
#else
    FD_SET(sock, rd_set);
#endif
    max_sock = sock;

    while(1)
    {
        rd_ready = *rd_set;
        wr_ready = *wr_set;

        gettimeofday(&act, NULL);

        Pthread_mutex_lock("loop timeout", &timeout_mutex);
        if ( ! timeout_clients.empty() )
        {
            msg.pdebug(D_TIME, "next time %d:%d", timeout_clients.begin()->first.tv_sec, timeout_clients.begin()->first.tv_usec);

            to.tv_sec  = timeout_clients.begin()->first.tv_sec - act.tv_sec;
            to.tv_usec = timeout_clients.begin()->first.tv_usec - act.tv_usec;
            if ( to.tv_usec < 0 )
            {
                to.tv_usec += 1000000;
                to.tv_sec--;
            }
            if ( to.tv_sec < 0 )
            {
                if ( timeout_clients.begin()->second->wait_for_timeout )
                {
                    to.tv_usec = 0;
                    to.tv_sec = 0;
                }
            }
            msg.pdebug(D_TIME, "timeout %d:%d", to.tv_sec, to.tv_usec);
        }
        else
        {
            to.tv_sec = -1;
            to.tv_usec = 0;
        }

        Pthread_mutex_unlock("timeout", &timeout_mutex);
        Pthread_mutex_unlock("mutex",&mutex);

        if ( to.tv_sec < 0 )
            rsel = select( max_sock+1, &rd_ready, &wr_ready, (fd_set *)0, NULL);
        else
            rsel = select( max_sock+1, &rd_ready, &wr_ready, (fd_set *)0, &to );

        Pthread_mutex_lock("after loop", &mutex);
        if ( rsel == 0 )
        {
            Timeouts::iterator i;
            std::vector<TimeoutClient *> t;
            std::vector<TimeoutClient *>::iterator ti;
            timeval w;
            timeval act;

            gettimeofday(&act, NULL);
            w.tv_sec  = timeout_clients.begin()->first.tv_sec;
            w.tv_usec = timeout_clients.begin()->first.tv_usec;

            for ( i = timeout_clients.begin(); i != timeout_clients.end(); ++i )
                if (i->first.tv_sec == w.tv_sec &&
                        i->first.tv_usec == w.tv_usec)
                    t.push_back(i->second);
                else
                    break;

            for ( ti = t.begin(); ti != t.end(); ++ti )
                (*ti)->increment();

            for ( ti = t.begin(); ti != t.end(); ++ti )
                (*ti)->timeout(act.tv_sec, act.tv_usec, w.tv_sec, w.tv_usec );

            continue;
        }
        else if ( rsel < 0 )
        {
            FD_ZERO(rd_set);
#if defined(__MINGW32__) || defined(__CYGWIN__)
            FD_SET((unsigned)sock, rd_set);
#else
            FD_SET(sock, rd_set);
#endif
            max_sock = sock;
            for ( i=clients.begin(); i != clients.end(); ++i )
            {
                 struct stat s;
                if ( fstat(i->first, &s) == 0 ) // @suppress("Invalid arguments")
                {
#if defined(__MINGW32__) || defined(__CYGWIN__)
                    FD_SET((unsigned)i->first, rd_set);
#else
                    FD_SET(i->first, rd_set);
#endif
                    if ( i->first > max_sock ) max_sock = i->first;
                }
                else
                {
                    msg.line("close %d",i->first);
                    ::close(i->first);
                    del_clients.push_back(i->first);
                }
            }
            continue;
        }

        for ( i = clients.begin(); rsel > 0 && i != clients.end() ; ++i )
        {
            if ( FD_ISSET(i->first, &wr_ready ) )
            {
                rsel--;
                i->second.write();
                if ( i->second.empty() )
                {
                    msg.pdebug(D_CON, "Client %d need_close: %d", i->first, i->second.need_close);
#if defined(__MINGW32__) || defined(__CYGWIN__)
                    FD_CLR((unsigned)i->first, wr_set );
#else
                    FD_CLR(i->first, wr_set );
#endif
                    if ( i->second.need_close )
                    {
                        ::close(i->first);
                        del_clients.push_back(i->first);
                    }
                }

                if ( FD_ISSET(i->first, &rd_ready) )
                    rsel--;
            }

            else if ( FD_ISSET(i->first, &rd_ready ) )
            {
                rsel--;
#if defined(__MINGW32__) || defined(__CYGWIN__)
                rval = ::recv( i->first, buffer, sizeof(buffer) - 1, 0);
#else
                rval = ::read( i->first, buffer, sizeof(buffer) - 1);
#endif
                if ( rval > 0 )
                {
                    buffer[rval] = '\0';
                    msg.pdebug(D_PROV, "request für SocketProvider %s client %d", i->second.p->getProvidername().c_str(), i->first);
                    i->second.p->request(i->first, buffer, rval);
                }
                else if ( rval == 0 )
                {

                    msg.pdebug(D_CON, "client %d disconnected", i->first );
                    ::close(i->first);
                    del_clients.push_back(i->first);
                }
                else
                {
#if defined(__MINGW32__) || defined(__CYGWIN__)
                    int lerror = WSAGetLastError();
                	if ( ( errno != EINTR && errno != 0 ) || lerror )
#else
                	if ( errno != EINTR )
#endif
                	{
                        msg.perror(E_CLIENT_READ, "Fehler beim lesen des clients %d",i->first);
#if defined(__MINGW32__) || defined(__CYGWIN__)
                        msg.line("Fehlernummer %d", WSAGetLastError());
#else
                        msg.line("%s", strerror(errno));
#endif
                        ::close(i->first);
#if defined(__MINGW32__) || defined(__CYGWIN__)
                        FD_CLR((unsigned)i->first, wr_set );
#else
                        FD_CLR(i->first, wr_set );
#endif
                        del_clients.push_back(i->first);
                    }
                }
            }
        }

        // Vieleicht ein neuer Client
        // ==========================

        if (  FD_ISSET(sock, &rd_ready ) )
        {
            rsel--;

            struct sockaddr_storage c;
#if defined (Darwin)
            socklen_t size = sizeof(c);
            if ( ( rval = accept(sock, (struct sockaddr *)&c, &size ) ) < 0 )
#elif defined(__MINGW32__) || defined(__CYGWIN__)
            int size = sizeof(c);
            if ( ( rval = accept(sock, (struct sockaddr *)&c, &size ) ) < 0 )
#else
            socklen_t size = sizeof(c);
            if ( ( rval = accept4(sock, (struct sockaddr *)&c, &size, SOCK_CLOEXEC ) ) < 0 )
#endif
            {
                msg.perror(E_ACCEPT, "Fehler beim accept - client kann nicht verbunden werden");
                msg.line("%s", strerror(errno));
            }
            else
            {
                SocketProvider *p;
                if ( ( p = get_provider("Http")) != NULL )
                {
                    clients[rval] = Client(this, p, rval, &c, size);
                    msg.pdebug(D_CON, "client %d connected: addr %s, port %s", rval, clients[rval].getHost().c_str(), clients[rval].getPort().c_str());
                }
                else
                {
                    ::close(rval);
                }

            }

            FD_ZERO(rd_set);
#if defined(__MINGW32__) || defined(__CYGWIN__)
            unsigned long cmd;
            cmd = 1;
            ioctlsocket(rval, FIONBIO, &cmd);
            FD_SET((unsigned)sock, rd_set);
#else
            FD_SET(sock, rd_set);
#endif
            max_sock = sock;
            for ( i=clients.begin(); i != clients.end(); ++i )
            {
#if defined(__MINGW32__) || defined(__CYGWIN__)
                FD_SET((unsigned)i->first, rd_set);
#else
                FD_SET(i->first, rd_set);
#endif
                if ( i->first > max_sock ) max_sock = i->first;
            }
        }

        // Clients dessen Verbindungen unterbrochen wurden
        // ===============================================

        if ( ! del_clients.empty() )
        {
            std::vector<int>::iterator ii;
            for ( ii = del_clients.begin(); ii != del_clients.end() ; ++ii )
            {
                std::map<std::string, SocketProvider*>::iterator p;
                for ( p = provider.begin(); p != provider.end(); ++p )
                    p->second->disconnect(*ii);

                clients.erase(clients.find(*ii));
            }


            del_clients.clear();

            FD_ZERO(rd_set);
#if defined(__MINGW32__) || defined(__CYGWIN__)
            FD_SET((unsigned)sock, rd_set);
#else
            FD_SET(sock, rd_set);
#endif
            max_sock = sock;
            for ( i=clients.begin(); i != clients.end(); ++i )
            {
#if defined(__MINGW32__) || defined(__CYGWIN__)
                FD_SET((unsigned)i->first, rd_set);
#else
                FD_SET(i->first, rd_set);
#endif
                if ( i->first > max_sock ) max_sock = i->first;
            }
        }
    }
}


