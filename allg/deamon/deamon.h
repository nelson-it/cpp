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
    int deamon;
public:
    Deamon();

    int isdeamon() { return deamon; }

};
#endif

#endif /* deamon_mne */
