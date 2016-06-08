#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iconv.h>

#include <map>

#include <utils/tostring.h>

#include "dbquery.h"
#include "dbhttputils_query.h"

std::map<std::string,std::string> DbHttpUtilsQuery::dateformat;
std::map<std::string,std::string> DbHttpUtilsQuery::days;

DbHttpUtilsQuery::DbHttpUtilsQuery(DbHttp *h, int noadd )
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

    subprovider["dyndata.xml"]  = &DbHttpUtilsQuery::dyndata_xml;
    subprovider["data.xml"]     = &DbHttpUtilsQuery::data_xml;
    subprovider["data.csv"]     = &DbHttpUtilsQuery::data_csv;

    resultcount = 0;

    if ( ! noadd )
       h->add_provider(this);
 }

DbHttpUtilsQuery::~DbHttpUtilsQuery()
{
}

void DbHttpUtilsQuery::mk_exportvalue(HttpHeader *h, DbConnect::Result r, int dpytyp, std::string colfs, std::string sep, std::string dateformat)
{
    if ( r.isnull )
    {
        fprintf(h->content, "%s", sep.c_str());
        return;
    }

    switch( dpytyp )
    {
    case DbConnect::BINARY:
        fprintf(h->content, "%s\"binary\"", sep.c_str());
        break;
    case DbConnect::DATE:
    {
        char str[64];
        struct tm tm;
        time_t t = (long)r;
        if ( localtime_r(&t,&tm) == NULL )
        {
            msg.pwarning(W_CONV, "Konnte <%d> nicht in einen Zeitstruktur wandeln");
            memset(&tm, 0, sizeof(tm));
        }
        strftime(str, sizeof(str), dateformat.c_str(),&tm);
        fprintf(h->content, "%s%s", sep.c_str(), str);
        break;
    }
    case DbConnect::TIME:
    {
        char str[64];
        struct tm tm;
        time_t t = (long)r;
        if ( localtime_r(&t,&tm) == NULL )
        {
            msg.pwarning(W_CONV, "Konnte <%d> nicht in einen Zeitstruktur wandeln");
            memset(&tm, 0, sizeof(tm));
        }
        strftime(str, sizeof(str), "%H:%M",&tm);
        fprintf(h->content, "%s%s", sep.c_str(), str);
    }
    break;
    case DbConnect::DATETIME:
    {
        char str[64];
        struct tm tm;
        long val = (long)r;

        time_t t = val;
        fprintf(stderr, "%s\n", r.format(&msg));
        if ( localtime_r(&t,&tm) == NULL )
        {
            msg.pwarning(W_CONV, "Konnte <%d> nicht in einen Zeitstruktur wandeln");
            memset(&tm, 0, sizeof(tm));
        }
        strftime(str, sizeof(str), (dateformat + " %H:%M").c_str(),&tm);
        fprintf(h->content, "%s%s", sep.c_str(), str);
    }
        break;
    case DbConnect::INTERVAL:
            fprintf(h->content, "%s%02ld:%02ld", sep.c_str(), ((long)(r)) / 3600, (((long)(r)) % 3600) / 60 );
        break;
    case DbConnect::DAY:
        fprintf(h->content, "%s\"%s\"", sep.c_str(), ToString::mkcsv( days[r.format(&msg, NULL, 0, colfs.c_str())]).c_str());
        break;
    default:
        fprintf(h->content, "%s\"%s\"", sep.c_str(), ToString::mkcsv( r.format(&msg, NULL, 0, colfs.c_str())).c_str());
    }
}

void DbHttpUtilsQuery::mk_export(HttpHeader *h)
{
     iconv_t iv;
     char *inbuf, *outbuf, *ci, *co;
     size_t innum,outnum;

     fseek( h->content, 0, SEEK_END);
     innum = ftell(h->content);
     fseek( h->content, 0, SEEK_SET);

#ifndef MACOS
     if ( innum < 0 ) innum = 0;
#endif
     ci = inbuf = new char[innum + 1];
     inbuf[innum] = '\0';
     fread(inbuf, innum, 1, h->content);

     fclose(h->content);
     h->content = fopen(h->content_filename.c_str(), "wb+");

     fseek (h->content, 0, SEEK_SET);

     co = outbuf = new char[innum * 4];
     outnum = ( innum * 4 - 1);

     iv = iconv_open("iso-8859-1//TRANSLIT", "utf-8");
     iconv (iv, &ci, &innum, &co, &outnum);
     iconv_close(iv);

     *co = '\0';
     fwrite(outbuf, (co - outbuf), 1, h->content);

     delete[] outbuf;
     delete[] inbuf;

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
void DbHttpUtilsQuery::dyndata_xml(Database *db, HttpHeader *h)
{
    std::string queryid;
    std::string colname, colid;
    long coltyp;
    std::string colformat;
    std::string::size_type i,j,k;
    std::string::size_type pos;

    DbConnect::Result     rr;
    DbConnect::ResultMat *r;
    DbConnect::ResultMat::iterator rm;

    DbQuery *query;
    std::vector<std::string::size_type> vals;
    std::vector<std::string> colfs;
    std::vector<std::string> colids;
    std::vector<long> dpytyp;
    std::string keyname;

    std::string dateformat;

    std::map<int, DbConnect::ResultMat> headmap;
    std::map<int, DbConnect::ResultMat>::iterator hi;
    std::set<DbConnect::Result> colset;
    std::set<DbConnect::Result>::iterator ci;

    int exports = ( h->vars["exports"] != "" );
    CsList hide(h->vars["tablehidecols"]);

    CsList cols(h->vars["cols"]);
    CsList sorts(h->vars["scols"]);
    CsList wval(h->vars["wval"]);
    CsList wcol(h->vars["wcol"]);
    CsList wop(h->vars["wop"]);

    h->status = 200;
    if ( exports )
        h->content_type = "application/mne_octet-stream";
    else
        h->content_type = "text/xml";

    if ( !exports )
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result>\n", h->charset.c_str());

    while ( wval.size() < wcol.size() ) wval.add("");
    while ( wop.size()  < wcol.size() ) wop.add("");

    resultcount = 0;
    query = db->p_getQuery();
    if ( h->vars["distinct"] != "" && ! cols.empty() )
        query->setName(h->vars["schema"], h->vars["query"], &cols, h->vars["unionnum"]);
    else
        query->setName(h->vars["schema"], h->vars["query"], NULL,  h->vars["unionnum"]);

    for ( i=0; i<3; ++i)
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

            colids.push_back(query->getId(pos));
            dpytyp.push_back(coltyp);
            colfs.push_back(colformat);
            if ( h->vars["distinct"] == "" ) vals.push_back(pos); else vals.push_back(0);

            if ( i == 1 )
                keyname = query->getName(pos);
        }
        else
        {
            msg.perror(E_WRONG_COLUMN, "Spaltenid <%s> unbekannt", cols[i].c_str());
        }
    }


    if ( ( h->vars["no_vals"] == "" || h->vars["no_vals"] == "false" )  && h->error_found == 0 )
    {
        for (query->start_cols(); query->getCols(&colid, &colname, &coltyp, &colformat);)
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



        r = query->select(&wcol, &wval, &wop, &sorts );
        if (h->vars["sqlend"] != "")
            db->p_getConnect()->end();

        resultcount = r->size();
        if (h->vars["lastquery"] != "" )
        {
            msg.pmessage(0,"Letze Abfrage:");
            msg.ignore_lang = 1;
            msg.line("%s", query->getLaststatement().c_str());
        }

        for (rm = r->begin(), j=0; rm != r->end(); ++rm, ++j)
        {
            if ( ( ci = colset.find((*rm)[vals[0]]) ) == colset.end() )
                colset.insert((*rm)[vals[0]]);
        }

        for (rm = r->begin(), j=0; rm != r->end(); ++rm )
        {
            if ( rr != ((*rm)[vals[1]]) )
            {
                rr = (*rm)[vals[1]];
                DbConnect::ResultMat v;
                headmap.insert(std::make_pair(j, v));
                hi = headmap.find(j++);
            }
            hi->second.push_back((*rm));
        }

        if ( headmap.size() == 0 ) return;

        if ( exports )
            fprintf(h->content,"\"%s\"", ToString::mkcsv(keyname).c_str());
        else
            fprintf(h->content, "<head>\n<d><id>%s</id><typ>%ld</typ><format>%s</format><name>%s</name><regexp><reg>%s</reg><help>%s</help><mod>%s</mod></regexp></d>\n", colids[1].c_str(), dpytyp[1], colfs[1].c_str(), ToString::mkxml(keyname.c_str()).c_str(), "", "", "");

        for ( k=3; k<cols.size(); ++k)
        {
            pos = query->ofind(cols[k]);
            if ( pos != std::string::npos )
            {
                colformat = query->getFormat(pos);
                coltyp = query->getColtyp(pos);
                if (  coltyp == DbConnect::FLOAT || coltyp == DbConnect::DOUBLE )
                {
                    std::string::size_type j;
                    if ( ( j = colformat.find('%',0) != std::string::npos ))
                        colformat.insert(j, 1, '\'');
                }

                if ( exports )
                {
                    char str[100];
                    snprintf(str, sizeof(str), "%ld", (long)k );
                    if ( hide.find(str) == std::string::npos )
                    {
                        fprintf(h->content,";\"%s\"",ToString::mkcsv(query->getName(pos)).c_str());
                    }
                }
                else
                    fprintf(h->content,
                            "<d><id>%s</id><typ>%ld</typ><format>%s</format><name>%s</name><regexp><reg>%s</reg><help>%s</help><mod>%s</mod></regexp></d>\n",
                            query->getId(pos).c_str(), coltyp, colformat.c_str(), query->getName(pos).c_str(),
                            query->getRegexp(pos).c_str(), query->getRegexphelp(pos).c_str(), query->getRegexpmod(pos).c_str());

                if ( h->vars["distinct"] == "" ) vals.push_back(pos); else vals.push_back(0);
                colfs.push_back(colformat);
                dpytyp.push_back(coltyp);

            }
            else
            {
                msg.perror(E_WRONG_COLUMN, "Spaltenid <%s> unbekannt", cols[i].c_str());
            }
        }

        for ( ci = colset.begin(), i=0; ci != colset.end(); ++ci, ++i )
        {
            DbConnect::Result rr = *ci;
            if ( exports )
                fprintf(h->content,";\"%s\"",ToString::mkxml(rr.format(&msg, NULL, 0, colfs[0].c_str())).c_str());
            else
                fprintf(h->content, "<d><id>%s%d</id><typ>%ld</typ><format>%s</format><name>%s</name><regexp><reg>%s</reg><help>%s</help><mod>%s</mod></regexp></d>\n", colids[0].c_str(), (int)i, dpytyp[2], colfs[2].c_str(), ToString::mkxml(rr.format(&msg, NULL, 0, colfs[0].c_str())).c_str(),"", "", "");
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

        if ( ! exports )
            fprintf(h->content, "</head><body>\n");
        else
        {
            DbHttpAnalyse::Client::Userprefs u = this->http->getUserprefs();
            dateformat = this->dateformat["de"];
            if ( u["region"] == "US" ) dateformat = this->dateformat["us"];
            else if ( u["language"] == "en") dateformat = this->dateformat["en"];
            else if ( u["language"] == "fr") dateformat = this->dateformat["fr"];
            fprintf(h->content,"\n");
        }

        for ( hi = headmap.begin(); hi != headmap.end(); ++hi )
        {
            for ( i = 0; i < hi->second.size(); )
            {
                if ( exports )
                {
                    mk_exportvalue(h, (hi->second[i])[vals[1]],  dpytyp[1], colfs[1], "", dateformat );
                }
                else
                {
                    if ( dpytyp[1] != DbConnect::BINARY )
                        fprintf(h->content, "<r><v>%s</v>", ToString::mkxml( (hi->second[i])[vals[1]].format(&msg, NULL, 0, colfs[1].c_str())).c_str());
                    else
                        fprintf(h->content, "<v>binary</v>");
                }

                for ( k=3; k<vals.size(); k++)
                {
                    if ( exports )
                    {
                        char str[100];
                        snprintf(str, sizeof(str), "%ld", (long)k );
                        if ( hide.find(str) == std::string::npos )
                        {
                            mk_exportvalue(h, (hi->second[i])[vals[k]],  dpytyp[k], colfs[k], ";" , dateformat );
                        }
                    }
                    else
                    {
                        if ( dpytyp[vals[k]] != DbConnect::BINARY )
                            fprintf(h->content, "<v>%s</v>", ToString::mkxml( (hi->second[i])[vals[k]].format(&msg, NULL, 0, colfs[k].c_str())).c_str());
                        else
                            fprintf(h->content, "<v>binary</v>");
                    }
                }

                for ( ci = colset.begin(); ci != colset.end(); ++ci )
                {
                    DbConnect::Result rr = *ci;
                    if ( hi->second.size() > i && hi->second[i][vals[0]] == rr )
                    {
                        if ( exports )
                        {
                            mk_exportvalue(h, (hi->second[i])[vals[2]],  dpytyp[2], colfs[2], ";", dateformat );
                        }
                        else
                        {
                            if ( dpytyp[0] != DbConnect::BINARY )
                                fprintf(h->content, "<v>%s</v>", ToString::mkxml( (hi->second[i])[vals[2]].format(&msg, NULL, 0, colfs[2].c_str())).c_str());
                            else
                                fprintf(h->content, "<v>binary</v>");
                        }
                        i++;
                    }
                    else
                    {
                        DbConnect::Result r;
                        if ( exports )
                            fprintf(h->content, ";");
                        else
                            fprintf(h->content, "<v></v>");
                    }
                }

                if ( exports )
                    fprintf(h->content, "\n");
                else
                    fprintf(h->content, "</r>");
            }
        }

        if ( ! exports )
            fprintf(h->content, "</body>");
        else
            mk_export(h);
    }
}

void DbHttpUtilsQuery::data_xml(Database *db, HttpHeader *h)
{
    std::string queryid;
    std::string colname, colid, regexp, regexphelp, regexpmod;
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


    if ( h->vars["dyndata"] != "" )
        return dyndata_xml(db, h);

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

    resultcount = 0;
    query = db->p_getQuery();
    if ( h->vars["distinct"] != "" && ! cols.empty() )
        query->setName(h->vars["schema"], h->vars["query"], &cols, h->vars["unionnum"]);
    else
        query->setName(h->vars["schema"], h->vars["query"], NULL,  h->vars["unionnum"]);

    if ( !exports )
        fprintf(h->content, "<?xml version=\"1.0\" encoding=\"%s\"?><result><head>\n", h->charset.c_str());

    if ( cols.empty() )
    {
        for (query->start_cols(); query->getCols(&colid, &colname, &coltyp, &colformat, &regexp, &regexphelp, &regexpmod);)
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
                        "<d><id>%s</id><typ>%ld</typ><format>%s</format><name>%s</name><regexp><reg>%s</reg><help>%s</help><mod>%s</mod></regexp></d>\n",
                        colid.c_str(), coltyp, colformat.c_str(), colname.c_str(),regexp.c_str(), regexphelp.c_str(), regexpmod.c_str());
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
                        "<d><id>%s</id><typ>%ld</typ><format>%s</format><name>%s</name><regexp><reg>%s</reg><help>%s</help><mod>%s</mod></regexp></d>\n",
                        query->getId(pos).c_str(), coltyp, colformat.c_str(), query->getName(pos).c_str(),
                        query->getRegexp(pos).c_str(), query->getRegexphelp(pos).c_str(), query->getRegexpmod(pos).c_str());
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
        for (query->start_cols(); query->getCols(&colid, &colname, &coltyp, &colformat);)
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
        resultcount = r->size();
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
                        mk_exportvalue(h, *rv,  dpytyp[i], colfs[i], komma, dateformat );
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
                            mk_exportvalue(h, (*rm)[vals[i]],  dpytyp[i], colfs[i], komma, dateformat );
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
        {
            fprintf(h->content, "</body>");
        }
        else
        {
            mk_export(h);
        }
    }
    else
    {
        if (h->vars["sqlend"] != "")
            db->p_getConnect()->end();

    }
    db->release(query);
    return;
}
