#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/tostring.h>

#include "dbquery.h"
#include "dbhttputils_query.h"

DbHttpUtilsQuery::DbHttpUtilsQuery(DbHttp *h)
:DbHttpProvider(h),
 msg("DbHttpUtilsQuery")
 {
    subprovider["data.xml"]  = &DbHttpUtilsQuery::data_xml;
    h->add_provider(this);
 }

DbHttpUtilsQuery::~DbHttpUtilsQuery()
{
}

int DbHttpUtilsQuery::request(Database *db, HttpHeader *h)
{

    SubProviderMap::iterator i;

    if ( ( i = subprovider.find(h->filename)) != subprovider.end() )
    {
        (this->*(i->second))(db, h);
        return 1;
    }

    return 0;

}

void DbHttpUtilsQuery::data_xml(Database *db, HttpHeader *h)
{
    std::string queryid;
    std::string colname, colid, regexp, regexphelp;
    long coltyp;
    std::string colformat;
    std::string::size_type i,j;
    std::string::size_type pos;

    DbConnect::ResultMat *r;
    DbQuery *query;
    std::vector<std::string::size_type> vals;
    std::vector<std::string> colfs;
    std::vector<long> dpytyp;

    if ( h->vars["dbtime"] != "" ) msg.pwarning(W_OLD, "dbtime existiert nicht mehr");

    h->status = 200;
    h->content_type = "text/xml";


    CsList cols(h->vars["cols"]);
    CsList sorts(h->vars["scols"]);
    CsList wval(h->vars["wval"]);
    CsList wcol(h->vars["wcol"]);
    CsList wop(h->vars["wop"]);

    while ( wval.size() < wcol.size() ) wval.add("");
    while ( wop.size()  < wcol.size() ) wop.add("");

    query = db->p_getQuery();
    if ( h->vars["distinct"] != "" && ! cols.empty() )
        query->setName(h->vars["schema"], h->vars["query"], &cols, h->vars["unionnum"]);
    else
        query->setName(h->vars["schema"], h->vars["query"], NULL,  h->vars["unionnum"]);

    fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><head>\n",
            h->charset.c_str());

    if ( cols.empty() )
    {
        for (query->start_cols(); query->getCols(&colid, &colname, &coltyp, &colformat, &regexp, &regexphelp);)
        {
            if ( coltyp == DbConnect::FLOAT || coltyp == DbConnect::DOUBLE )
            {
                if ( ( i = colformat.find('%',0) != std::string::npos ))
                    colformat.insert(i, 1, '\'');
            }
            if ( colid[0] != '-' )
            {
                fprintf(h->content,
                        "<d><id>%s</id><typ>%ld</typ><format>%s</format><name>%s</name><regexp><reg>%s</reg><help>%s</help></regexp></d>\n",
                        colid.c_str(), coltyp, colformat.c_str(), colname.c_str(),regexp.c_str(), regexphelp.c_str());
                colfs.push_back(colformat);
                dpytyp.push_back(coltyp);
            }
        }
    }
    else
    {
        for ( i=0; i<cols.size(); ++i)
        {
            pos = query->ofind(cols[i]);
            if ( pos != std::string::npos )
            {
                colformat = query->getFormat(pos);
                coltyp = query->getColtyp(pos);
                if (  coltyp == DbConnect::FLOAT || coltyp == DbConnect::DOUBLE )
                {
                    if ( ( j = colformat.find('%',0) != std::string::npos ))
                        colformat.insert(j, 1, '\'');
                }

                fprintf(h->content,
                        "<d><id>%s</id><typ>%ld</typ><format>%s</format><name>%s</name><regexp><reg>%s</reg><help>%s</help></regexp></d>\n",
                        query->getId(pos).c_str(), coltyp, colformat.c_str(), query->getName(pos).c_str(),
                        query->getRegexp(pos).c_str(), query->getRegexphelp(pos).c_str());
                if ( h->vars["distinct"] == "" ) vals.push_back(pos); else vals.push_back(0);
                colfs.push_back(colformat);
                dpytyp.push_back(coltyp);
            }
            else
            {
                msg.perror(E_WRONG_COLUMN, "Spaltenid <%s> unbekannt", cols[i].c_str());
            }

        }
        if ( h->vars["distinct"] != "" && h->error_found == 0 )
        {
            int found;
            i=0;
            for (query->start_cols(); query->getCols(&colid, &colname, &coltyp, &colformat);)
            {
                found = 0;
                cols.reset();
                while ( ( j = cols.find(colid,1)) != std::string::npos )
                {
                    vals[j] = i;
                    found = 1;
                }

                if ( found ) ++i;
            }
        }
    }

    fprintf(h->content, "</head>");

    if ( ( h->vars["no_vals"] == "" || h->vars["no_vals"] == "false" )  && h->error_found == 0 )
    {
        for (query->start_cols(); query->getCols(&colid, &colname, &coltyp,
                &colformat);)
        {
            if (h->vars.exists(colid + "Input.old") )
            {
                wcol.add(colid);
                if ( h->vars.exists(colid + "Op.old") )
                    wop.add(h->vars[colid + "Op.old"]);
                else
                    wop.add("=");
                wval.add(h->vars[colid + "Input.old"]);
            }
        }


        DbConnect::ResultMat::iterator rm;
        DbConnect::ResultVec::iterator rv, re;

        r = query->select(&wcol, &wval, &wop, &sorts );
        if (h->vars["sqlend"] != "")
            db->p_getConnect()->end();

        if (h->vars["lastquery"] != "" )
        {
            msg.pmessage(0,"Letze Abfrage:");
            msg.ignore_lang = 1;
            msg.line("%s", query->getLaststatement().c_str());
        }

        fprintf(h->content, "<body>\n");
        for (rm = r->begin(); rm != r->end(); ++rm)
        {
            fprintf(h->content, "<r>");
            rv = (*rm).begin();
            re = (*rm).end();
            if ( cols.empty() )
            {
                for ( i=0,j=0 ; rv != re; ++rv, ++i)
                {
                    if ( dpytyp[i] != DbConnect::BINARY )
                        fprintf(h->content, "<v>%s</v>", ToString::mkxml( rv->format(&msg, NULL, 0, colfs[i].c_str())).c_str());
                    else
                        fprintf(h->content, "<v>binary</v>");
                }
            }
            else
            {
                for ( i=0; i<vals.size(); i++)
                {
                    if ( dpytyp[i] != DbConnect::BINARY )
                        fprintf(h->content, "<v>%s</v>", ToString::mkxml( (*rm)[vals[i]].format(&msg, NULL, 0, colfs[i].c_str())).c_str());
                    else
                        fprintf(h->content, "<v>binary</v>");
                }
            }
            fprintf(h->content, "</r>\n");
        }
        fprintf(h->content, "</body>");
    }
    else
    {
        if (h->vars["sqlend"] != "")
            db->p_getConnect()->end();

    }
    db->release(query);
    return;
}

