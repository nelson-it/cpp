#ifndef wake_mne
#define wake_mne

#include <message/message.h>

class Wake
{
    Message msg;
    enum ERROR_TYPES
    {
        E_SOCKET = 1,
        E_HOST,
        E_BROADCAST,
        E_SEND,
        E_ETHER
    };

public:
    Wake(const char *host, const char *ether);
};
#endif
