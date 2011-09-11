#ifndef puran2_mne
#define puran2_mne

#include <utils/cdll.h>

#ifdef __cplusplus
extern "C"
{
#endif

DLLIMPORT int puran2_init(char *filename, int wrap);

DLLIMPORT unsigned long puran2_random();
DLLIMPORT int           puran2_seek(long seek);
DLLIMPORT void          puran2_stop();

DLLIMPORT unsigned long puran2_random();
DLLIMPORT long          puran2_long();

DLLIMPORT char*         puran2_get_error();
DLLIMPORT int          puran2_get_errnum();
 
#ifdef __cplusplus
}
#endif

#endif /* puran2_mne */
