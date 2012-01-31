#ifndef php_mne
#define php_mne

#include "process.h"
#include <embedweb/http_header.h>

class PhpExec : private Process
{
public:
    PhpExec(  std::string cmd, HttpHeader *h );
    ~PhpExec();

};
#endif
