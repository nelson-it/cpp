#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dbtable.h"

#include "dbhttputils_lang.h"

DbHttpUtilsLanguage::DbHttpUtilsLanguage(DbHttp *h)
          :DbHttpProvider(h),
	   msg("DbHttpUtilsLanguage")
{
    subprovider["get.js"]     = &DbHttpUtilsLanguage::get;
    subprovider["get.html"]   = &DbHttpUtilsLanguage::get;

    h->add_provider(this);
}

DbHttpUtilsLanguage::~DbHttpUtilsLanguage()
{
}

int DbHttpUtilsLanguage::request(Database *db, HttpHeader *h)
{

    SubProviderMap::iterator i;

    if ( ( i = subprovider.find(h->filename)) != subprovider.end() )
    {

	if ( h->filename.find(".html") == strlen(h->filename.c_str()) - 5 )
	{
	    this->http->js_start(h);
	    (this->*(i->second))(db, h);
	    this->http->js_end(h);

	    h->content_type = "text/html";
	}
	else
	{
	    (this->*(i->second))(db, h);
	}

	return 1;
    }

    return 0;

}

void DbHttpUtilsLanguage::get( Database *db, HttpHeader *h)
{

    DbTable *t;

    std::string user;
    std::vector<std::string>vals;
    DbTable::ValueMap where;
    CsList order;
    DbConnect::ResultMat *r;

    std::string lang;
    std::string categorie;
    std::string str;

    h->status = 200;
    h->content_type = "text/plain";

    t   = db->p_getTable(db->getApplschema(), "userpref");

    vals.push_back("language");
    vals.push_back("htmldatestyle");

    where["username"] = db->p_getConnect()->getUser();

    r = t->select(&vals, &where, NULL, &order, 0, 1);
    if ( ! r->empty() )
    {
	str = (char*)((*r)[0][0]);
	add_content(h,  "top.mneMain.language = '%s';\n", str.c_str());

	if ( (std::string)((*r)[0][1]) != "" ) str = (char*)((*r)[0][1]);
	add_content(h,  "top.mneMain.dateformat='%s';\n", str.c_str());
    }

    db->release(t);
    return;
}
