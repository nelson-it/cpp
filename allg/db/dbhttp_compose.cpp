#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <utils/tostring.h>
#include <db/dbquery_creator.h>
#include "dbhttp_compose.h"

DbHttpCompose::DbHttpCompose(DbHttp *h) :
               DbHttpUtilsQuery(h, 1), msg("DbHttpCompose")
{
	subprovider["weblet.json"] = &DbHttpCompose::weblet_json;
	subprovider["subweblets.json"] = &DbHttpCompose::subweblets_json;
	subprovider["select.json"] = &DbHttpCompose::select_json;

	h->add_provider(this);
}

DbHttpCompose::~DbHttpCompose()
{
}

int DbHttpCompose::request(Database *db, HttpHeader *h)
{
    SubProviderMap::iterator i;

    if ( ( i = subprovider.find(h->filename)) != subprovider.end() )
    {
        (this->*(i->second))(db, h);

        if (h->vars["sqlend"] != "")
            db->p_getConnect()->end();

        return 1;
    }
    return 0;
}

void DbHttpCompose::weblet_json(Database *db, HttpHeader *h)
{
    std::string name = h->vars["name"];

    h->vars.clear();
    h->vars.setVar("schema", "mne_application");
    h->vars.setVar("query", "weblet");
    h->vars.setVar("cols", "template,label,htmlcomposeid");
    h->vars.setVar("wcol", "name");
    h->vars.setVar("wop", "=");
    h->vars.setVar("wval", name );
    h->vars.setVar("sqlend", "1" );

    DbHttpUtilsQuery::data_json(db, h);
}

void DbHttpCompose::subweblets_json(Database *db, HttpHeader *h)
{
    std::string htmlcomposeid = h->vars["htmlcomposeid"];

    h->vars.clear();
    h->vars.setVar("schema", "mne_application");
    h->vars.setVar("query", "weblet_tabs");
    h->vars.setVar("cols", "htmlcomposetabid,id,position,subposition,loadpos,path,initpar,depend,label");
    h->vars.setVar("scols","position,loadpos,subposition");
    h->vars.setVar("wcol", "htmlcomposeid,groupexists");
    h->vars.setVar("wop", "=,=");
    h->vars.setVar("wval", htmlcomposeid + ",1" );
    h->vars.setVar("distinct", "1" );

    h->vars.setVar("sqlend", "1" );

    DbHttpUtilsQuery::data_json(db, h);
}

void DbHttpCompose::select_json(Database *db, HttpHeader *h)
{
    std::string htmlcomposeid = h->vars["htmlcomposeid"];
    CsList ids(h->vars["ids"]);
    unsigned int i;

    std::string wval, wop, wcol;

    wop = "("; wcol = wval = "";
    for ( i = 0; i< ids.size(); ++i)
    {
        wcol += "id,";
        wop  += "=,o";
        wval += ids[i] + ",";
    }

    wop = wop.substr(0, wop.length() - 2) + "),";
    h->vars.clear();
    h->vars.setVar("schema", "mne_application");
    h->vars.setVar("query", "weblet_select");
    h->vars.setVar("cols", "element,schema,query,tab,wcol,wop,wval,cols,scols,showcols,showids,showalias,weblet,selval,type");
    h->vars.setVar("wcol", wcol + "htmlcomposeid");
    h->vars.setVar("wop", wop + "=");
    h->vars.setVar("wval", wval +  htmlcomposeid );
    h->vars.setVar("sqlend", "1" );

    DbHttpUtilsQuery::data_json(db, h);
}
