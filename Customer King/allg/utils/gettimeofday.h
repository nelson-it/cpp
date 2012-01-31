#ifndef gettimeofday_mne
#define gettimeofday_mne

#if defined(__MINGW32__) || defined(__CYGWIN__)

#include <winsock2.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0

struct timezone {
     int tz_minuteswest; /* minutes W of Greenwich */
     int tz_dsttime;     /* type of dst correction */
};
int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif /* 0 */

struct tm* localtime_r (const time_t *clock, struct tm *result);

#ifdef __cplusplus
}
#endif

#endif /* __MINGW32__ */

#endif /* gettimeofday_mne */
