#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>


#if defined(__MINGW32__) || defined(__CYGWIN__)

#include <winsock2.h>
#include <windows.h>
#include <time.h>

#include "gettimeofday.h"

#if 0
#define EPOCHFILETIME (116444736000000000LL)

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    FILETIME        ft;
    LARGE_INTEGER   li;
    __int64         t;
    static int      tzflag;

    if (tv)
    {
	GetSystemTimeAsFileTime(&ft);
	li.LowPart  = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	t  = li.QuadPart;       /* In 100-nanosecond intervals */
	t -= EPOCHFILETIME;     /* Offset to the Epoch time */
	t /= 10;                /* In microseconds */
	tv->tv_sec  = (long)(t / 1000000);
	tv->tv_usec = (long)(t % 1000000);
    }

    if (tz)
    {
	if (!tzflag)
	{
	    _tzset();
	    tzflag++;
	}
	tz->tz_minuteswest = _timezone / 60;
	tz->tz_dsttime = _daylight;
    }

return 0;
}

static pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;

struct tm* localtime_r (const time_t *clock, struct tm *result) {
	pthread_mutex_lock(&time_mutex);
	if (!clock || !result) return NULL;
	memcpy(result,localtime(clock),sizeof(*result));
	pthread_mutex_unlock(&time_mutex);
	return result;
}
#endif

#endif
