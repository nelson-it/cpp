#ifndef dbconstraint_error_mne
#define dbconstraint_error_mne

#include <string>
#include <map>

#include "database.h"

class DbConstraintError
{
public:

    typedef std::map<std::string, std::string> ConstraintMessageMap;
    typedef std::map<std::string, ConstraintMessageMap> ConstraintMessageMaps;

    static ConstraintMessageMaps messages;

public:
    DbConstraintError();

    void read(Database *db, int sqlend = 1);
    std::string get(std::string lang, std::string id);

};


#endif /* dbconstraint_error_mne */

