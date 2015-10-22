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

    enum WARNING_TYPES
    {
        W_PHP = Process::W_MAX
    };

    enum MESSAGE_TYPES
    {
        M_PHP = Process::M_MAX
    };
public:
    PhpExec(  std::string cmd, HttpHeader *h );
    ~PhpExec();

};
#endif
