#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <argument/argument.h>

#include "dbconnect.h"
#include "dbtranslate.h"
#include "dbtable.h"

#include "dbhttp_application.h"
#include "dbhttp_provider.h"

class DbHttpApplicationSingle: public DbHttpProvider
{
    std::string path;
public:
    DbHttpApplicationSingle( DbHttp *h, std::string path )
      : DbHttpProvider(h)
    {
        this->path = path;
        h->add_provider(this);
    }

    virtual ~DbHttpApplicationSingle()
    {
    }

    virtual std::string getPath()
    {
        return path;
    }
    virtual int request(Database *db, HttpHeader *h)
    {
       h->dirname = "/";
       h->filename = this->path + ".html";
       h->status = 1000;
       return 1;
    }

};


DbHttpApplication::DbHttpApplication( DbHttp *h, Database *db )
{
    DbConnect *c = db->p_getConnect();
    DbConnect::ResultMat *r;
    DbConnect::ResultMat::iterator i;

    c->execute("SELECT DISTINCT menuname FROM mne_application.menu", 1 );
    r = c->p_get_result();
    for ( i = r->begin(); i != r->end(); ++i )
        this->appls.push_back( new DbHttpApplicationSingle(h, (std::string)((*i)[0])) );
}

DbHttpApplication::~DbHttpApplication()
{
    unsigned int i;
    for ( i =0; i<this->appls.size(); ++i)
        delete this->appls[i];
}
