#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

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
        std::string str;
        unsigned int i;
        struct stat s;

        h->dirname = "/";
        h->filename = this->path + ".html";
        for (i=0; i<h->serverpath.size(); i++)
        {
            str = h->serverpath[i] + "/" + h->dirname + "/" + h->filename;
            if (stat(str.c_str(), &s) == 0 )
            {
                contentf(h, str.c_str());
                return 1;
            }
        }

        return 0;
    }

};


DbHttpApplication::DbHttpApplication( DbHttp *h, Database *db )
{
    Argument a;
    Database *dbapp = db->getDatabase();
    DbConnect *c = dbapp->p_getConnect("", a["DbTranslateUser"], a["DbTranslatePasswd"]);

    DbConnect::ResultMat *r;
    DbConnect::ResultMat::iterator i;

    if ( c != NULL )
    {
        c->execute("SELECT DISTINCT menuname FROM mne_application.menu", 1 );
        r = c->p_get_result();
        for ( i = r->begin(); i != r->end(); ++i )
            this->appls.push_back( new DbHttpApplicationSingle(h, (std::string)((*i)[0])) );
    }

    delete dbapp;
}

DbHttpApplication::~DbHttpApplication()
{
    unsigned int i;
    for ( i =0; i<this->appls.size(); ++i)
        delete this->appls[i];
}
