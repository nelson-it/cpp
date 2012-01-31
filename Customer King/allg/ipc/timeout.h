#ifndef timeout_mne
#define timeout_mne

#include <sys/time.h>
#include <utils/gettimeofday.h>
#include "s_socket.h"

class TimeoutClient : public timeval
{
     friend class ServerSocket;
     ServerSocket *s;

     struct timeval wakeup;
     struct timeval interval;
     int wait_for_timeout;

public:

     TimeoutClient( ServerSocket *s,
                    long i_sec = 0, long i_usec = 0,
		    long w_sec = 0, long w_usec  = 0)
     {
	this->s = s;

        this->wakeup.tv_sec = 0;
	this->wakeup.tv_usec = 0;
	this->interval.tv_sec = 0;
	this->interval.tv_usec = 0;

	setInterval(i_sec, i_usec);
	if ( w_sec > 0 || w_usec > 0 )
	    setWakeup(w_sec, w_usec);
    }

    virtual ~TimeoutClient()
    {
	if ( s == NULL ) return;
	s->del_timeout(this);
    }

    virtual void timeout( long sec, long usec, long w_sec, long w_usec ) = 0;

    void setWakeup( long sec, long usec = 0, long interval = 1)
    {
	if ( s == NULL ) return;

	if ( interval == 0 ) interval = 1;
	else usec = 0;

	s->del_timeout(this);

	this->wakeup.tv_sec = sec - sec % interval;
	this->wakeup.tv_usec = usec;

	while ( this->wakeup.tv_usec >= 1000000 ) 
	{
	    this->wakeup.tv_sec++;
	    this->wakeup.tv_usec -= 1000000;
	}

	if ( this->wakeup.tv_sec > 0 || this->wakeup.tv_usec > 0 )
	{
	    this->tv_sec = sec;
	    this->tv_usec = usec;
	    this->wait_for_timeout = 1;
	}
	else
	{
	   this->tv_sec = 0;
	   this->tv_usec = 0;
           this->wait_for_timeout = 0;
	}

	s->msg.pdebug(ServerSocket::D_TIME, "setWakeup: %d:%d %d:%d %d:%d",
	              this->tv_sec, this->tv_usec,
		      this->wakeup.tv_sec, this->wakeup.tv_usec,
		      this->interval.tv_sec, this->interval.tv_usec );

	s->add_timeout(this);
	
    }
    
    void setInterval( long sec, long usec = 0 )
    {

	if ( s == NULL )
	    return;

	s->del_timeout(this);

        this->interval.tv_sec = sec;
	this->interval.tv_usec = usec;

	this->tv_sec = 0;
	this->tv_usec = 0;

	if ( sec != 0 || usec != 0 )
	{
	    if ( this->wakeup.tv_sec == 0 )
	    {
		gettimeofday(this, NULL);
		this->tv_usec = 0;
	    }
	    else
	    {
		this->tv_sec = wakeup.tv_sec;
		this->tv_usec = wakeup.tv_usec;
	    }

	    this->tv_sec  += sec;
	    this->tv_usec += usec;

	    this->wait_for_timeout = 1;
	}
	else
	{
	    this->tv_sec = 0;
	    this->tv_usec = 0;
	    this->wait_for_timeout = 0;
	}

	s->add_timeout(this);

    }

    void increment()
    {

	if ( s == NULL ) return;
	
	s->del_timeout(this);

	this->wakeup.tv_sec = 0;
	this->wakeup.tv_usec = 0;
	this->wait_for_timeout = 0;

        if ( this->interval.tv_sec == 0 && this->interval.tv_usec == 0 )
	{
	    this->tv_sec = 0;
	    this->tv_usec = 0;
	}
	else
	{
	    this->tv_sec += this->interval.tv_sec;
	    this->tv_usec += this->interval.tv_usec;
            this->wait_for_timeout = 1;

	    if ( this->tv_usec >= 1000000 ) 
	    {
	        this->tv_sec++;
		this->tv_usec -= 1000000;
	    }
	}

	s->add_timeout(this);

    }
    
};

#endif /* timeout_mne */
