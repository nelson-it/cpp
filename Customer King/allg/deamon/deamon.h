#ifndef deamon_mne
#define deamon_mne

#if defined(__MINGW32__) || defined(__CYGWIN__)
class Deamon
{

};
#else
#include <string>

#include <message/message.h>

class Deamon
{
    Message msg;
public:
    Deamon();

};
#endif

#endif /* deamon_mne */
