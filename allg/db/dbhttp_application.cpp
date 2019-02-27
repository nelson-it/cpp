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
    Message msg;
    enum E_ERROR
    {
        E_FILE = 1
    };

    std::string path;
    std::string appl;

public:
    DbHttpApplicationSingle( DbHttp *h, std::string path, std::string appl )
      : DbHttpProvider(h),
        msg("DbHttpProvider")
    {
        this->path = path;
        this->appl = appl;
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
        for (i=0; i<h->serverpath.size(); i++)
        {
            str = h->serverpath[i] + "/" + h->dirname + "/index.tmpl" ;
            if (stat(str.c_str(), &s) == 0 )
            {
                FILE *fp;
                int size;
                char *buffer;

                h->content_length = 0;
                if ((fp = fopen(str.c_str(), "rb")) == NULL)
                {
                    msg.perror(E_FILE, "konnte Datei <%s> nicht öffnen", str.c_str());
                    return 0;
                }

                 fseek(fp, 0, SEEK_END);
                 size = ftell(fp);
                 fseek(fp, 0, SEEK_SET);

                 if (size < 0) size = 0;

                 buffer = new char[size+1];
                 fread(buffer, size, 1, fp);
                 fclose(fp);
                 buffer[size] = '\0';
                 str = buffer;
                 delete[] buffer;

                 size_t pos = 0;
                 while ((pos = str.find("####Appl####", pos)) != std::string::npos) {
                      str.replace(pos, 12, this->appl);
                      pos += this->appl.length();
                 }

                 add_content(h, str);
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
            this->appls.push_back( new DbHttpApplicationSingle(h, (std::string)((*i)[0]), (std::string)((*i)[0])) );

    }

    delete dbapp;
}

DbHttpApplication::~DbHttpApplication()
{
    unsigned int i;
    for ( i =0; i<this->appls.size(); ++i)
        delete this->appls[i];
}
