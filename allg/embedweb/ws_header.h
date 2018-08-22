#ifndef ws_header_mne
#define ws_header_mne

#include "http_header.h"

class WsHeader : public HttpHeader
{
    friend class Ws;

    std::string opcode;

    unsigned char *origdata;
    unsigned long origdata_length;

    unsigned char *data;
    unsigned long data_length;

public:
    WsHeader ()
    {
        origdata = data = NULL;
        origdata_length = data_length = 0;
    };
    virtual ~WsHeader()
    {
    }

    std::string getOpcode() { return opcode; }
};

#endif /* ws_header_mne */
