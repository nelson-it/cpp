#include "fdsize.h"

#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef Darwin
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
    fd = -1;

    buffer = NULL;
    length = 0;
    index = 0;

    host = 0;
    port = 0;

    need_close = 0;
}

ServerSocket::Client::Client( ServerSocket *s, int fd, struct sockaddr_in *sin)
{
    this->s = s;
    this->fd = fd;

    buffer = NULL;
    length = 0;
    index = 0;

    host = sin->sin_addr.s_addr;
    port = sin->sin_port;

    need_close = 0;
#if ! ( defined(__MINGW32__) || defined(__CYGWIN__) )
    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        s->msg.perror(E_SOCK_OPEN, "konnte reuse Option nicht setzen");
        s->msg.line("%s", strerror(errno));
    }
#endif

}

ServerSocket::Client::Client( const Client &in)
{
    s = in.s;
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

    need_close = in.need_close;
}

ServerSocket::Client &ServerSocket::Client::operator=( const Client &in)
{

    s = in.s;
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

    need_close = in.need_close;

    return *this;
}

ServerSocket::Client::~Client()
{
    if ( buffer != NULL ) delete buffer;
}

void ServerSocket::Client::write(char *b, int l)
{

    if ( buffer != NULL )
    {
        char *tmp;
        tmp = new char[length - index + l];
        memcpy(tmp, &buffer[index], length - index);
        memcpy(&tmp[length - index], b, l);

        delete buffer;

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

    s->msg.pdebug(ServerSocket::D_CON, "sende zu client %d: %d bytes",
            fd, l);

    if ( buffer != NULL )
    {
        char *tmp;
        tmp = new char[length - index + l];
        memcpy(tmp, &buffer[index], length - index);
        fread(&tmp[length - index], l, 1, f);

        delete buffer;

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
            s->msg.perror(ServerSocket::E_WRITE,
                    "Fehler beim Schreiben zum Client %d\n", fd);
            s->msg.line("%s", strerror(errno));
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
            s->msg.pdebug(ServerSocket::D_CON, "daten zu client %d gesendet",
                    fd);
            delete buffer;
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
    struct sockaddr_in server;

    if ( ( sock = socket(AF_INET, SOCK_STREAM, 0 ) ) < 0 )
    {
        msg.perror(E_SOCK_OPEN, "konnte keinen socket für den Service öffnen");
        msg.line("%s", strerror(errno));
        exit(1);
    }

#if ! ( defined(__MINGW32__) || defined(__CYGWIN__) )
    int on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        msg.perror(E_SOCK_OPEN, "konnte reuse Option nicht setzen");
        msg.line("%s", strerror(errno));
    }
#endif

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(socketnum);

    length = sizeof(server);

    if ( ( rval = bind(sock, (struct sockaddr *)&server, length)) < 0 )
    {
        msg.perror(E_SOCK_BIND,"konnte socket nich an port %d binden", socketnum);
        msg.line("%s", strerror(errno));
        exit(1);
    }

    providerid = 1;
    http = NULL;

#ifdef PTHREAD
    pthread_mutex_init(&mutex,NULL);
    pthread_mutex_init(&timeout_mutex,NULL);
#endif

}

ServerSocket::~ServerSocket()
{
    ::close(sock);
}

void ServerSocket::add_provider( SocketProvider *p)
{
    std::string name;

#ifdef PTHREAD
    Pthread_mutex_lock("add provider", &mutex);
#endif

    name = p->getProvidername();

    if ( pnum.find(name) == pnum.end() )
    {
        msg.pdebug(D_PROV, "SocketProvider \"%s\" wird hinzugefügt",
                name.c_str());
        pnum[p->getProvidername()] = providerid;
        provider[providerid] = p;
        if ( name == "Http" )
            http = p;
    }
    else
    {
        msg.perror(E_PRO_EXISTS, "SocketProvider \"%s\" ist schon registriert",
                name.c_str());
    }

#ifdef PTHREAD
    Pthread_mutex_unlock("mutex", &mutex);
#endif
}

void ServerSocket::del_provider( SocketProvider *p)
{
    std::string name;

#ifdef PTHREAD
    Pthread_mutex_lock("del provider", &mutex);
#endif

    name = p->getProvidername();

    msg.pdebug(D_PROV, "SocketProvider \"%s\" wird entfernt", name.c_str());
    if ( pnum.find(name) != pnum.end() )
    {
        provider.erase(pnum[name]);
        pnum.erase(name);
        if ( name == "Http" )
            http = NULL;
    }
    else
    {
        msg.perror(E_PRO_NOT_EXISTS,
                "SocketProvider \"%s\" ist nicht registriert",
                name.c_str());
    }

#ifdef PTHREAD
    Pthread_mutex_unlock("mutex", &mutex);
#endif
}

void ServerSocket::add_timeout( TimeoutClient *t)
{
#ifdef PTHREAD
    Pthread_mutex_lock("add_timeout", &timeout_mutex);
#endif

    timeout_clients.insert(std::pair<timeval, TimeoutClient *>(*t,t));

#ifdef PTHREAD
    Pthread_mutex_unlock("timeout", &timeout_mutex);
#endif
}

void ServerSocket::del_timeout( TimeoutClient *t)
{
    Timeouts::iterator i;
    int n;

#ifdef PTHREAD
    Pthread_mutex_lock("del timeout", &timeout_mutex);
#endif

    i = timeout_clients.find(*(timeval *)t);
    for ( n = timeout_clients.count(*(timeval *)t); n > 0; ++i, --n )
    {
        if ( i->second == t )
        {
            timeout_clients.erase(i);
            break;
        }
    }

#ifdef PTHREAD
    Pthread_mutex_unlock("timeout",&timeout_mutex);
#endif
}

void ServerSocket::write(int client, char *buffer, int size)
{
    Clients::iterator i;

#ifdef PTHREAD
    Pthread_mutex_lock("write char", &mutex);
#endif

    if ( ( i = clients.find(client)) != clients.end() )
    {
        msg.pdebug(D_RD, "Schreibe zu Client %d %d bytes", client, size);

        i->second.write(buffer, size);

#ifndef PTHREAD

#ifdef __MINGW32__
        FD_SET((unsigned)client, wr_set);
#else
        FD_SET(client, wr_set);
#endif

#endif

    }
    else
    {
        msg.pdebug(D_CON, "Client %d hat die Verbindung während "
                "des Schreibens beendet", client);
    }

#ifdef PTHREAD
    Pthread_mutex_unlock("mutex", &mutex);
#endif
}

void ServerSocket::write(int client, FILE *fp, int size)
{
    Clients::iterator i;

#ifdef PTHREAD
    Pthread_mutex_lock("write fp", &mutex);
#endif

    if ( ( i = clients.find(client)) != clients.end() )
    {
        msg.pdebug(D_RD, "Schreibe zu Client %d %d bytes", client, size);

        i->second.write(fp, size);
#ifndef PTHREAD

#ifdef __MINGW32__
        FD_SET((unsigned)client, wr_set);
#else
        FD_SET(client, wr_set);
#endif

#endif

    }
    else
    {
        msg.pdebug(D_CON, "Client %d hat die Verbindung während "
                "des Schreibens beendet", client);
    }

#ifdef PTHREAD
    Pthread_mutex_unlock("mutex", &mutex);
#endif
}

void ServerSocket::flush( int client )
{
    Clients::iterator i;

#ifdef PTHREAD
    Pthread_mutex_lock("write fp", &mutex);
#endif

    if ( ( i = clients.find(client)) != clients.end() )
    {
        msg.pdebug(D_RD, "Flush Client %d", client);

#ifdef PTHREAD
        i->second.write_all();
        if ( i->second.need_close )
            ::close(i->first);
#endif
    }
    else
    {
        msg.pdebug(D_CON, "Client %d hat die Verbindung während "
                "des Schreibens beendet", client);
    }

#ifdef PTHREAD
    Pthread_mutex_unlock("mutex", &mutex);
#endif
}


int ServerSocket::read(int client, char *buffer, int size)
{
    int l, len;
    fd_set rd;

    msg.pdebug(D_CON, "Client %d liest noch %d werte", client, size );

#ifdef PTHREAD
    Pthread_mutex_lock("read", &mutex);
#endif

    FD_ZERO(&rd);
    for ( len = 0; len != size; )
    {

#ifdef __MINGW32__
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
#ifdef PTHREAD
            Pthread_mutex_unlock("mutex", &mutex);
#endif

            msg.pdebug(D_CON, "Client %d hat die Verbindung während "
                    "des Lesens beendet", client );
            return -1;
        }
        else
        {
            if ( errno != EINTR )
            {
#ifdef PTHREAD
                Pthread_mutex_unlock("mutex", &mutex);
#endif

                msg.pdebug(D_CON, "Client %d hat einen Fehler während "
                        "des Lesens", client );
                msg.line("Grund %s", strerror(errno));
                return -1;
            }
        }
    }

#ifdef PTHREAD
    Pthread_mutex_unlock("mutex", &mutex);
#endif

    return size;
}

void ServerSocket::close(int client)
{
    Clients::iterator i;

#ifdef PTHREAD
    Pthread_mutex_lock("close", &mutex);
#endif

    if ( ( i = clients.find(client)) != clients.end() )
    {
        if ( ! i->second.empty() )
        {
            msg.pdebug(D_CON, "Anforderung zum Verbindungende zu Client %d",
                    client);
            i->second.need_close = 1;

#ifdef PTHREAD
            Pthread_mutex_unlock("mutex",&mutex);
#endif

            return;
        }

        msg.pdebug(D_CON, "Verbindung zum Client %d wird beendet", client);

        std::map<int, SocketProvider*>::iterator ii;

        ::close(i->first);
        clients.erase(i);

        for ( ii = provider.begin(); ii != provider.end(); ++ii )
            ii->second->disconnect(i->first);

        FD_ZERO(rd_set);
#ifdef __MINGW32__
        FD_SET((unsigned)sock, rd_set);
#else
        FD_SET(sock, rd_set);
#endif
        max_sock = sock;
        for ( i=clients.begin(); i != clients.end(); ++i )
        {
#ifdef __MINGW32__
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

#ifdef PTHREAD
    Pthread_mutex_unlock("mutex",&mutex);
#endif

}

void ServerSocket::loop()
{
    Clients::iterator i;
    std::vector<int> del_clients;

    struct timeval act, to;
    fd_set rd_ready;
    fd_set wr_ready;

#ifdef PTHREAD
    Pthread_mutex_lock("loop", &mutex);
#endif

    // Umständliches Konstrukt um die Grösse verbergen zu können
    // =========================================================
    fd_set rd_set_local;
    fd_set wr_set_local;

    rd_set = &rd_set_local;
    wr_set = &wr_set_local;

    char buffer[10240];
    int rval,rsel,have_disconnect;

    listen(sock, 5);

    FD_ZERO(rd_set);
    FD_ZERO(wr_set);

#ifdef __MINGW32__
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

#ifdef PTHREAD
        Pthread_mutex_lock("loop timeout", &timeout_mutex);
#endif

        if ( ! timeout_clients.empty() )
        {
            msg.pdebug(D_TIME, "next time %d:%d",
                    timeout_clients.begin()->first.tv_sec,
                    timeout_clients.begin()->first.tv_usec);

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

#ifdef PTHREAD
        Pthread_mutex_unlock("timeout", &timeout_mutex);
        Pthread_mutex_unlock("mutex",&mutex);
#endif

        if ( to.tv_sec < 0 )
            rsel = select( max_sock+1, &rd_ready, &wr_ready, (fd_set *)0, NULL);
        else
            rsel = select( max_sock+1, &rd_ready, &wr_ready, (fd_set *)0, &to );

#ifdef PTHREAD
        Pthread_mutex_lock("after loop", &mutex);
#endif

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
            //msg.perror(E_SELECT, "Fehler beim select - wird ignoriert");
            //msg.line("%s", strerror(errno));
            FD_ZERO(rd_set);
#ifdef __MINGW32__
            FD_SET((unsigned)sock, rd_set);
#else
            FD_SET(sock, rd_set);
#endif
            max_sock = sock;
            for ( i=clients.begin(); i != clients.end(); ++i )
            {
                struct stat s;
                if ( fstat(i->first, &s) == 0 )
                {
#ifdef __MINGW32__
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

        have_disconnect = 0;
        for ( i = clients.begin(); rsel > 0 && i != clients.end() ; ++i )
        {
            if ( FD_ISSET(i->first, &wr_ready ) )
            {
                rsel--;
                i->second.write();
                if ( i->second.empty() )
                {
                    msg.pdebug(D_CON, "Client %d need_close: %d",
                            i->first, i->second.need_close);
#ifdef __MINGW32__
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
#ifdef __MINGW32__
                rval = ::recv( i->first, buffer, sizeof(buffer) - 1, 0);
#else
                rval = ::read( i->first, buffer, sizeof(buffer) - 1);
#endif
                if ( rval > 0 )
                {
                    buffer[rval] = '\0';
                    if ( 1 || *buffer != '\0' )
                    {
                        msg.pdebug(D_PROV, "request für SocketProvider http "
                                "client %d", i->first);

                        if ( http != NULL )
                            http->request(i->first, buffer, rval);
                        else
                            msg.perror(E_HTTP_NULL,
                                    "Besitze keinen HttpSocketProvider");
                    }
                    else
                    {
                        msg.perror(E_PRO_UNKNOWN, "SocketProvider ist unbekannt");
                    }
                }
                else if ( rval == 0 )
                {

                    msg.pdebug(D_CON, "client %d disconnected", i->first );
                    ::close(i->first);
                    del_clients.push_back(i->first);
                }
                else
                {
                    if ( errno != EINTR )
                    {
                        msg.perror(E_CLIENT_READ,
                                "Fehler beim lesen des clients %d",i->first);
                        msg.line("%s", strerror(errno));
                        ::close(i->first);
#ifdef PTHREAD
#else
#ifdef __MINGW32__
                        FD_CLR((unsigned)i->first, wr_set );
#else
                        FD_CLR(i->first, wr_set );
#endif
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

            struct sockaddr_in c;
#if defined(__MINGW32__) || defined(__CYGWIN__)
            int size = sizeof(c);
#else
            socklen_t size = sizeof(c);
#endif
            if ( ( rval = accept(sock, (struct sockaddr *)&c, &size ) ) < 0 )
            {
                msg.perror(E_ACCEPT, "Fehler beim accept - "
                        "client kann nicht verbunden werden");
                msg.line("%s", strerror(errno));
            }
            else
            {
                clients[rval] = Client(this, rval, &c);
                msg.pdebug(D_CON, "client %d connected: addr %x, port %d",
                        rval, clients[rval].getHost(),
                        clients[rval].getPort());
            }

            FD_ZERO(rd_set);
#ifdef __MINGW32__
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
#ifdef __MINGW32__
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
                std::map<int, SocketProvider*>::iterator p;
                for ( p = provider.begin(); p != provider.end(); ++p )
                    p->second->disconnect(*ii);

                clients.erase(clients.find(*ii));
            }


            del_clients.clear();

            FD_ZERO(rd_set);
#ifdef __MINGW32__
            FD_SET((unsigned)sock, rd_set);
#else
            FD_SET(sock, rd_set);
#endif
            max_sock = sock;
            for ( i=clients.begin(); i != clients.end(); ++i )
            {
#ifdef __MINGW32__
                FD_SET((unsigned)i->first, rd_set);
#else
                FD_SET(i->first, rd_set);
#endif
                if ( i->first > max_sock ) max_sock = i->first;
            }
        }
    }
}


