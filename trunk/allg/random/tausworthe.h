#ifndef mne_tausworthe_h
#define mne_tausworthe_h

#include <utils/cdll.h>

#ifdef __cplusplus
extern "C"
{
#endif

	DLLIMPORT int tausworthe_init(unsigned long i0, unsigned long i1, unsigned long i2, 
		             unsigned long i3, unsigned long i4);
	DLLIMPORT unsigned long tausworthe_random();
	DLLIMPORT long          tausworthe_long();
	
	DLLIMPORT unsigned long tausworthe_puran2_random();
	DLLIMPORT long          tausworthe_puran2_long();
#ifdef __cplusplus
}
#endif

#endif /*mne_tausworthe_h*/
