#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils/tostring.h>

#include "dbquery.h"
#include "dbhttputils_query.h"

std::map<std::string,std::string> DbHttpUtilsQuery::dateformat;
std::map<std::string,std::string> DbHttpUtilsQuery::days;

DbHttpUtilsQuery::DbHttpUtilsQuery(DbHttp *h)
:DbHttpProvider(h),
 msg("DbHttpUtilsQuery")
 {
    if ( dateformat.empty() )
    {
        dateformat["de"] = "%d.%m.%Y";
        dateformat["en"] = "%d/%m/%Y";
        dateformat["us"] = "%m/%d/%Y";
        dateformat["fr"] = "%d-%m-%Y";

        days["1"] = msg.get("Sonntag");
        days["2"] = msg.get("Montag");
        days["3"] = msg.get("Dienstag");
        days["4"] = msg.get("Mittwoch");
        days["5"] = msg.get("Donnerstag");
        days["6"] = msg.get("Freitag");
        days["7"] = msg.get("Samstag");

    }

    subprovider["data.xml"]  = &DbHttpUtilsQuery::data_xml;
    subprovider["data.csv"]  = &DbHttpUtilsQuery::data_csv;
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
void DbHttpUtilsQuery::data_csv(Database *db, HttpHeader *h)
{
    h->vars.setVars("&exports=1");
    data_xml(db,h);
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

    std::string komma;
    int exports = ( h->vars["exports"] != "" );
    CsList hide(h->vars["tablehidecols"]);
    std::string dateformat;

    if ( h->vars["dbtime"] != "" ) msg.pwarning(W_OLD, "dbtime existiert nicht mehr");

    h->status = 200;
    if ( exports )
        h->content_type = "application/mne_octet-stream";
    else
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

    if ( !exports )
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><head>\n", h->charset.c_str());

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
                if ( exports )
                    fprintf(h->content,"%s\"%s\"",komma.c_str(),ToString::mkcsv(colname).c_str());
                 else
                    fprintf(h->content,
                        "<d><id>%s</id><typ>%ld</typ><format>%s</format><name>%s</name><regexp><reg>%s</reg><help>%s</help></regexp></d>\n",
                        colid.c_str(), coltyp, colformat.c_str(), colname.c_str(),regexp.c_str(), regexphelp.c_str());
                colfs.push_back(colformat);
                dpytyp.push_back(coltyp);
                komma = ";";
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

                if ( exports )
                {
                    char str[100];
                    snprintf(str, sizeof(str), "%ld", (long)i );
                    if ( hide.find(str) == std::string::npos )
                    {
                        fprintf(h->content,"%s\"%s\"",komma.c_str(),ToString::mkcsv(query->getName(pos)).c_str());
                        komma = ";";
                    }
                }
                 else
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

    if ( exports )
        fprintf(h->content,"\n");
     else
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

        if ( ! exports )
            fprintf(h->content, "<body>\n");
        else
        {
            DbHttpAnalyse::Client::Userprefs u = this->http->getUserprefs();
            dateformat = this->dateformat["de"];
            if ( u["region"] == "US" ) dateformat = this->dateformat["us"];
            else if ( u["language"] == "en") dateformat = this->dateformat["en"];
            else if ( u["language"] == "fr") dateformat = this->dateformat["fr"];
        }
        for (rm = r->begin(); rm != r->end(); ++rm)
        {
            komma = "";
            if ( ! exports )
                fprintf(h->content, "<r>");
            rv = (*rm).begin();
            re = (*rm).end();
            if ( cols.empty() )
            {
                for ( i=0,j=0 ; rv != re; ++rv, ++i)
                {
                    if ( exports )
                    {
                       switch( dpytyp[i] )
                       {
                       case DbConnect::BINARY:
                           fprintf(h->content, "%s\"binary\"", komma.c_str());
                           break;
                       case DbConnect::DATE:
                       {
                           char str[64];
                           struct tm tm;
                           time_t t = (long)(*rv);
                           strftime(str, sizeof(str), dateformat.c_str(),localtime_r(&t,&tm));
                           fprintf(h->content, "%s%s", komma.c_str(), str);
                           break;
                       }
                       case DbConnect::TIME:
                       {
                           char str[64];
                           struct tm tm;
                           time_t t = (long)(*rv);
                           strftime(str, sizeof(str), "%H:%M",localtime_r(&t,&tm));
                           fprintf(h->content, "%s%s", komma.c_str(), str);
                       }
                       break;
                       case DbConnect::DATETIME:
                       {
                           char str[64];
                           struct tm tm;
                           time_t t = (long)(*rv);
                           strftime(str, sizeof(str), (dateformat + " %H:%M").c_str(),localtime_r(&t,&tm));
                           fprintf(h->content, "%s%s", komma.c_str(), str);
                       }
                           break;
                       case DbConnect::INTERVAL:
                           fprintf(h->content, "%s%02ld:%02ld", komma.c_str(), ((long)(*rv)) / 3600, (((long)(*rv)) % 3600) / 60 );
                           break;
                       case DbConnect::DAY:
                           fprintf(h->content, "%s\"%s\"", komma.c_str(), ToString::mkcsv( days[rv->format(&msg, NULL, 0, colfs[i].c_str())]).c_str());
                           break;
                       default:
                           fprintf(h->content, "%s\"%s\"", komma.c_str(), ToString::mkcsv( rv->format(&msg, NULL, 0, colfs[i].c_str())).c_str());
                       }
                        komma = ";";
                    }
                    else
                    {
                        if ( dpytyp[i] != DbConnect::BINARY )
                            fprintf(h->content, "<v>%s</v>", ToString::mkxml( rv->format(&msg, NULL, 0, colfs[i].c_str())).c_str());
                        else
                            fprintf(h->content, "<v>binary</v>");
                    }
                }
            }
            else
            {
                for ( i=0; i<vals.size(); i++)
                {
                    if ( exports )
                    {
                        char str[100];
                        snprintf(str, sizeof(str), "%ld", (long)i );
                        if ( hide.find(str) == std::string::npos )
                        {
                            switch( dpytyp[i] )
                            {
                            case DbConnect::BINARY:
                                fprintf(h->content, "%s\"binary\"", komma.c_str());
                                break;
                            case DbConnect::DATE:
                            {
                                char str[64];
                                struct tm tm;
                                time_t t = (long)((*rm)[vals[i]]);
                                strftime(str, sizeof(str), dateformat.c_str(),localtime_r(&t,&tm));
                                fprintf(h->content, "%s%s", komma.c_str(), str);
                                break;
                            }
                            case DbConnect::TIME:
                            {
                                char str[64];
                                struct tm tm;
                                time_t t = (long)((*rm)[vals[i]]);
                                strftime(str, sizeof(str), "%H:%M",localtime_r(&t,&tm));
                                fprintf(h->content, "%s%s", komma.c_str(), str);
                            }
                            break;
                            case DbConnect::DATETIME:
                            {
                                char str[64];
                                struct tm tm;
                                time_t t = (long)((*rm)[vals[i]]);
                                strftime(str, sizeof(str), (dateformat + " %H:%M").c_str(),localtime_r(&t,&tm));
                                fprintf(h->content, "%s%s", komma.c_str(), str);
                            }
                                break;
                            case DbConnect::INTERVAL:
                                fprintf(h->content, "%s%02ld:%02ld", komma.c_str(), ((long)((*rm)[vals[i]])) / 3600, (((long)((*rm)[vals[i]])) % 3600) / 60 );
                                break;
                            case DbConnect::DAY:
                                fprintf(h->content, "%s\"%s\"", komma.c_str(), ToString::mkcsv( days[(*rm)[vals[i]].format(&msg, NULL, 0, colfs[i].c_str())]).c_str());
                                break;
                            default:
                                fprintf(h->content, "%s\"%s\"", komma.c_str(), ToString::mkcsv( (*rm)[vals[i]].format(&msg, NULL, 0, colfs[i].c_str())).c_str());
                            }
                            komma = ";";
                        }
                    }
                    else
                    {

                        if ( dpytyp[i] != DbConnect::BINARY )
                            fprintf(h->content, "<v>%s</v>", ToString::mkxml( (*rm)[vals[i]].format(&msg, NULL, 0, colfs[i].c_str())).c_str());
                        else
                            fprintf(h->content, "<v>binary</v>");
                    }
                }
            }
            if ( exports )
                fprintf(h->content, "\n");
            else
                fprintf(h->content, "</r>\n");
        }
        if ( ! exports )
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

