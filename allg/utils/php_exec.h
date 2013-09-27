#ifndef php_mne
#define php_mne

#include "process.h"
#include <embedweb/http_header.h>

class PhpExec : private Process
{
    enum ERROR_TYPES
    {
        E_PHP = Process::E_MAX
    };
public:
    PhpExec(  std::string cmd, HttpHeader *h );
    ~PhpExec();

};
#endif
