#ifndef pgconstraint_mne
#define pgconstraint_mne

#include <message/message.h>

#include "../dbconstraint.h"
#include "pgconnect.h"

class PgConstraint : private PgConnect, public DbConstraint
{
    Message msg;
    std::string schema;
    std::string table;

    enum ERROR_TYPES
    {
      NO_TABLENAME = 1
    };

public:
    PgConstraint( PgConnect *con);
    virtual ~PgConstraint() {};

    int create_check( std::string schema, std::string table,
                      std::string name, std::string check_clause,
                      std::string text_de, std::string text_en, int custom,
		      int ready = 0 );

    int create_primary(std::string schema, std::string table,
                       std::string name, std::string cols,
                       std::string text_de, std::string text_en, int custom,
                       int ready = 0);

    int create_foreign(std::string schema, std::string table,
                       std::string name, std::string cols,
		               std::string rschema, std::string rtable,
		               std::string rcols,
		               std::string text_de, std::string text_en, int custom,
		               int ready  = 0);

    int remove( std::string schema, std::string table,
                std::string name, int ready = 0 );
};

#endif /* pgconstraint_mne */

