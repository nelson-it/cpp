#ifndef dbconstraint_mne
#define dbconstraint_mne

#include <string>
#include <map>

#include <message/message.h>

class DbConstraint
{
public:

    typedef std::map<std::string, std::string> ConstraintMap;
    ConstraintMap constraint;

    class Foreign
    {
    public:
        std::string schema;
        std::string table;
        std::string cols;
	std::string rcols;

	void clear()
	{
	    schema = table = cols = rcols = "";
	}
    };

    typedef std::map<std::string, Foreign> ForeignMap;
    ForeignMap foreign;

public:
    virtual ~DbConstraint() {};

    virtual int create_check( std::string schema, std::string table,
			      std::string name, std::string check_clause, std::string text_de, std::string text_en, int custom,
			      int ready = 0) = 0;

    virtual int create_primary( std::string schema, std::string table,
			        std::string name, std::string cols, std::string text_de, std::string text_en, int custom,
			        int ready = 0) = 0;

    virtual int create_foreign( std::string schema, std::string table,
			        std::string name, std::string cols,
			        std::string rschema, std::string rtable,
				    std::string rcols,
				    std::string text_de, std::string text_en, int custom,
				    int ready = 0) = 0;

    virtual int remove( std::string schema, std::string table,
                        std::string name, int ready = 0 ) = 0;


};


#endif /* dbconstraint_mne */

