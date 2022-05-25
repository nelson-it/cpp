#ifndef dbhttp_application_mne
#define dbhttp_application_mne

#include <vector>
#include "dbhttp_provider.h"

class DbHttp;
class DbHttpApplicationSingle;

class DbHttpApplication
{
    std::vector<DbHttpApplicationSingle*> appls;

public:
    DbHttpApplication( DbHttp *http, Database *db );
    virtual ~DbHttpApplication();

};

#endif /* dbhttp_application_mne */
