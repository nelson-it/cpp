#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "dbtable.h"
#include "dbconstraint_error.h"

DbConstraintError::ConstraintMessageMaps DbConstraintError::messages;

DbConstraintError::DbConstraintError()
{
}

void DbConstraintError::read(Database *db, int sqlend )
{
    DbTable *tab = db->p_getTable(db->getApplschema(), "tableconstraintmessages");
    CsList cols("tableconstraintmessagesid,text_de,text_en");
    DbTable::ValueMap w;

    DbConnect::ResultMat *r = tab->select(&cols, &w);
    DbConnect::ResultMat::iterator rm;
    if ( sqlend ) tab->end();

    messages.clear();
    for (rm = r->begin(); rm != r->end(); ++rm)
    {
        messages["de"][((char*)(*rm)[0])] = ((char*)(*rm)[1]);
        messages["en"][((char*)(*rm)[0])] = ((char*)(*rm)[2]);
    }
}

std::string DbConstraintError::get(std::string lang, std::string id)
{
    DbConstraintError::ConstraintMessageMap::iterator i;
    if ( lang == "de" || lang == "en" )
    {
        i = messages[lang].find(id);
        if ( i != messages[lang].end() ) return i->second;
    }

    return "";
}
