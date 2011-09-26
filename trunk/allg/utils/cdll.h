#ifndef cdll_h_included
#define cdll_h_included

/* 
 * When building the DLL code, you should define BUILDING_DLL so that
 * the variables/functions are exported correctly. When using the DLL,
 * do NOT define BUILDING_DLL, and then the variables/functions will
 * be imported correctly.
 *
 * You need to be using egcs-1.1.1 or newer.
 *
 * Building the DLL:
 *  - define BUILDING_DLL, which defines DLLIMPORT __attribute__((dllexport))
 * Building the client code:
 *  - DO NOT define BUILDING_DLL, which defines DLLIMPORT to be one 
 *    __attribute__((dllimport))
 */

#ifdef WINDOWS

#if BUILDING_DLL
# define DLLIMPORT __declspec (dllexport)
#else /* Not BUILDING_DLL */
# define DLLIMPORT __declspec (dllimport)
#endif /* Not BUILDING_DLL */

#else

# define DLLIMPORT 
# define DLLEXPORT

# ifndef O_BINARY
#  define O_BINARY 0
# endif

#endif 

#endif /* cdll_h_included */

