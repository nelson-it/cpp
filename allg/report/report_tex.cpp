#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include <argument/argument.h>
#include <db/dbquery.h>
#include <db/dbconnect.h>
#include <utils/tostring.h>
#include <xml/xmltext_tex.h>

#include "report_tex.h"

ReportTex::ReportTex() :
    msg("ReportText")
    {

    Argument a;
    std::vector<std::string>::iterator i;
    std::string projectroot;

    path.setString((char *) a["RepRoot"],':');
    projectroot = (char*)a["projectroot"];

    for ( i=path.begin(); i != path.end(); ++i )
    {
       if ( (*i)[0] != '/' )
           (*i) = projectroot + "/" + (*i);
    }


    df = msg.getDateformat();
    tf = msg.getTimeformat();
    intf = msg.getIntervalformat();
    stf = msg.getTimesformat();
    sintf = msg.getIntervalsformat();

    dtf = df + " " + tf;
    sdtf = df + " " + stf;
    }

ReportTex::~ReportTex()
{
}

std::string ReportTex::getFile(std::string file, int is_dir )
{
    unsigned int i;
    struct stat s;

    for ( i=0; i<path.size(); i++ )
    {
        if ( (stat ((path[i] + "/" + file).c_str(), &s) == 0) && ((is_dir && S_ISDIR (s.st_mode)) || (! is_dir && S_ISREG (s.st_mode))) )
            break;
    }
    if ( i != path.size() )
        return (path[i] + "/" + file);
    return "";
}

int ReportTex::mk_report(Database *db, std::string reportname, int subreport,
        FILE *out, std::string language, std::string schema, std::string queryname,
        CsList *wcol, CsList *wval, CsList *wop, CsList *s, Macros *macros,
        Macros *xml)
{
    DbQuery * query;
    DbTable *reptab;
    std::string rtemplate;
    std::string root;

    DbTable::ValueMap w;
    CsList cols;
    CsList sort;

    Macros::iterator mi;

    DbConnect::ResultMat rm,prm;
    DbConnect::ResultMat::iterator iprm;
    FILE *fp, *pre, *post;

    char buffer[10240];
    unsigned int i, n, size;
    std::string str;
    std::string id, name, format;
    long typ;
    long landscape;

    std::vector<std::string> ids;
    std::vector<long> typs;
    std::vector<std::string> formats;

    std::vector<std::string> sortlist;
    std::map<unsigned int, unsigned int> sortid;
    std::string langid("de");
    std::string langsave;
    CsList      repcols;
    int have_query;
    unsigned int record_count;

    record_count = 0;

    langsave = msg.getLang();
    if (language != "" && language != langsave )
       msg.setLang(language);
    langid = msg.getLang();

    fprintf(out, "\\gdef\\mnelangid{%s}%%\n", langid.c_str());

    reptab = db->p_getTable(db->getApplschema(), "reports");
    w["name"] = reportname;
    cols.setString("title_" + langid
            + ",schema,query,sort,only_with_rows,template,landscape,cols,parentcols,ops,repcols");
    rm = *(reptab->select(&cols, &w));

    if (rm.size() == 0)
    {
        msg.perror(E_NOREPORT, "Report %s nicht vorhanden", reportname.c_str());
        db->release(reptab);
        msg.setLang(langsave);
        return record_count;
    }

    rtemplate = (char*) (rm[0][5]);
    if (rtemplate == "")
        rtemplate = reportname;

    if (s == NULL || s->getString() == "")
        sort.setString((char *) (rm[0][3]));
    else
        sort = *s;

    landscape = (long) (rm[0][6]);
    if ( ! subreport ) this->landscape = landscape;

    repcols.setString((char *) (rm[0][10]));

    if ( getFile(rtemplate, 1) == "" )
    {
    	msg.perror(E_NOREPORTROOT, "Reportroot ist nicht vorhanden");
    	return record_count;
    }

    have_query = 1;
    query = db->p_getQuery();
    if ( schema != "" && queryname != "" )
    {
    	query->setName((char*) schema.c_str(), (char*) queryname.c_str(), NULL );
    }
    else if ( (std::string) (rm[0][2]) != "" )
    {
    	query->setName((char*) (rm[0][1]), (char*) (rm[0][2]), ( ! repcols.empty()) ? &repcols : NULL );
    }
    else
    {
        have_query = 0;
    }

    msg.stop_debug();

    if ( !subreport && ((std::string)(rm[0][7])) != "" )
    {
        unsigned int i;
        CsList pwcol((char*) (rm[0][7]), ';');
        CsList pwval((char*) (rm[0][8]), ';');
        CsList pwop ((char*) (rm[0][9]), ';');

        for ( i = 0; i<pwcol.size(); ++i )
        {
            wcol->add(pwcol[i]);
            wop->add(pwop[i]);
            if ((pwval[i])[0] == '#')
                wval->add(pwval[i].substr(1));
            else
                wval->add(pwval[i]);
        }
    }

    if ( have_query )
    {
        prm = *(query->select(wcol, wval, wop, &sort));
        iprm = prm.begin();
        db->p_getConnect()->end();
    }

    if ((long) (rm[0][4]) != 0  && iprm == prm.end())
    {
        db->release(reptab);
        db->release(query);
        msg.setLang(langsave);
        return -1;
    }

    if ( iprm != prm.end() && !subreport )
    {
        std::map<std::string, std::string>::iterator ui;
        for ( ui = userprefs.begin(); ui != userprefs.end(); ++ui)
        {
            if ( query->find(ui->first) != std::string::npos  )
            {
                userprefs[ui->first] = ((*iprm)[query->find(ui->first)]).format(&msg);
            }
        }
        for ( ui = userprefs.begin(); ui != userprefs.end(); ++ui)
            fprintf(out, "\\gdef\\upref%s{%s}%%\n", ui->first.c_str(), ui->second.c_str());
    }

    if ( (iprm != prm.end()) && query->find("language") != std::string::npos  && !subreport )
    {
    	std::string l = (std::string)((*iprm)[query->find("language")]);
    	if ( l != "" && l != langid )
    	{
    		msg.setLang((char *)((*iprm)[query->find("language")]));
    		langid = msg.getLang();
    		fprintf(out, "\\gdef\\mnelangid{%s}%%\n", langid.c_str());
    	}
    }

    if (!subreport)
    {
        str = getFile(rtemplate + "/init.tex");
        if ( str == "" ) str = getFile("/allg/default/init.tex");
        if ( str != "" ) fp = fopen(str.c_str(), "r"); else fp = NULL;

        if ( fp == NULL )
        {
            db->release(query);
            db->release(reptab);
            msg.perror(E_NOINIT, "kann datei %s nicht finden", (rtemplate + "/init.tex").c_str());
            msg.setLang(langsave);
            return record_count;
        }

        if ( landscape )
            fprintf(out,"\\gdef\\mnelandscape{landscape}%%\n");

        while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
            fwrite(buffer, size, 1, out);

        fclose(fp);

        if ( iprm != prm.end() )
        {
            std::map<std::string, std::string>::iterator ui;
            for ( ui = userprefs.begin(); ui != userprefs.end(); ++ui)
            {
                if ( query->find(ui->first) != std::string::npos  )
                {
                    userprefs[ui->first] = (std::string)((*iprm)[query->find(ui->first)]);
                }
            }
            for ( ui = userprefs.begin(); ui != userprefs.end(); ++ui)
                fprintf(out, "\\gdef\\upref%s{%s}%%\n", ui->first.c_str(), ui->second.c_str());
        }

        fprintf(out, "\\gdef\\reptitle{%s}%%\n", ToString::mktex((char*) (rm[0][0])).c_str());

        str = getFile(rtemplate + "/docinit.tex");
        if ( str == "" ) str = getFile("/allg/default/docinit.tex");
        if ( str != "" ) fp = fopen(str.c_str(), "r"); else fp = NULL;

        if ( fp == NULL )
        {
            db->release(query);
            db->release(reptab);
            msg.perror(E_NOINIT, "kann datei %s nicht finden", (rtemplate + "/docinit.tex").c_str());
            msg.setLang(langsave);
            return record_count;
        }

        while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
            fwrite(buffer, size, 1, out);

        fclose(fp);


    }
    else
    {
        fprintf(out, "\\gdef\\subreptitle{%s}%%\n", ToString::mktex((char*) (rm[0][0])).c_str());

        str = getFile(rtemplate + "/subinit.tex");
        if ( str != "" ) fp = fopen(str.c_str(), "r"); else fp = NULL;

        if (fp != NULL)
        {
            while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
                fwrite(buffer, size, 1, out);
            fclose(fp);
        }
    }

    query->start_cols();
    n = 0;
    while (query->getCols(&id, &name, &typ, &format))
    {
    	if ( id[0] != '-' )
    	{
    		str = "i";
    		for (i = 0; i < sort.size(); ++i)
    			if (sort[i] == id)
    			{
    				if ( repcols.empty() )
    					sortid[i] = n;
    				else
    					sortid[i] = repcols.find(id);

    				fprintf(out, "\\gdef\\repgrouptitle%s{%s}%%\n", str.c_str(), ToString::mktex(id).c_str());
    				str = str + "i";
    				break;
    			}

    		if ( repcols.empty() || repcols.find(id) != std::string::npos )
    		{
    			ids.push_back("B" + id);
    			typs.push_back(typ);
    			formats.push_back(format);
    			fprintf(out, "\\gdef\\H%s{%s}%%\n", ToString::mktexmacro(id).c_str(), ToString::mktex(name).c_str());
    		}
    		n++;
    	}
    }

    if (macros != NULL)
        for (mi = macros->begin(); mi != macros->end(); ++mi)
            fprintf(out, "\\gdef\\%s{%s}%%\n", mi->first.c_str(), mi->second.c_str());

    if (xml != NULL)
        for (mi = xml->begin(); mi != xml->end(); ++mi)
        {
            XmlTextTex xml;
            xml.setXml(mi->second);
            fprintf(out, "\\gdef\\%s{", mi->first.c_str());
            xml.mk_output(out);
            fprintf(out, "}%%\n");
        }

    str = getFile(rtemplate + "/header.tex");
    if ( str != "" ) fp = fopen(str.c_str(), "r"); else fp = NULL;

    if (fp == NULL)
    {
        db->release(query);
        db->release(reptab);
        msg.perror(E_NOHEADER, "kann datei %s nicht finden", (rtemplate + "/header.tex").c_str());
        msg.setLang(langsave);
        return record_count;
    }

    while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
        fwrite(buffer, size, 1, out);

    fclose(fp);

    str = getFile(rtemplate + "/record.tex");
    if ( str != "" ) fp = fopen(str.c_str(), "r"); else fp = NULL;

    if (fp == NULL)
    {
        db->release(query);
        db->release(reptab);
        msg.perror(E_NORECORD, "kann datei %s nicht finden", (rtemplate + "/record.tex").c_str());
        msg.setLang(langsave);
        return record_count;
    }

    str = getFile(rtemplate + "/prerecord.tex");
    if ( str != "" ) pre = fopen(str.c_str(), "r"); else pre = NULL;

    str = getFile(rtemplate + "/postrecord.tex");
    if ( str != "" ) post = fopen(str.c_str(), "r"); else post = NULL;

    for (i = 0; i < sort.size(); ++i)
        sortlist.push_back("###############");

    for ( ; iprm != prm.end(); ++iprm, record_count++ )
    {
        if (pre != NULL)
        {
            fseek(pre, SEEK_SET, 0);
            while ((size = fread(buffer, 1, sizeof(buffer), pre)) > 0)
                fwrite(buffer, size, 1, out);
        }

        for (i = 0; i < sort.size(); ++i)
            w.clear();
        w["parent"] = reportname;
        w["before"] = 1;
        cols.setString("name,cols,parentcols,ops");
        sort.setString("num");
        rm = *(reptab->select(&cols, &w, NULL, &sort));

        for (i = 0; i < rm.size(); ++i)
        {
            CsList cols((char*) (rm[i][1]), ';');
            CsList pcols((char*) (rm[i][2]), ';');
            CsList pvals("", ';');
            CsList wops((char*) (rm[i][3]), ';');
            unsigned int n;

            for (n = 0; n < pcols.size(); ++n)
            {
                std::vector<std::string>::iterator f;

                if ((pcols[n])[0] == '#')
                    pvals.add(pcols[n].substr(1));
                else if ( (f = std::find(ids.begin(), ids.end(), "B" + pcols[n])) != ids.end())
                    pvals.add(((*iprm)[f - ids.begin()]).format());
                else
                    pvals.add("");
            }

            mk_report(db, (char*) (rm[i][0]), 1, out, langid, "", "", &cols, &pvals, &wops,
                    NULL);
        }

        for (str = "i", i = 0; i < sortlist.size(); ++i)
        {
            if (sortlist[i] != (*iprm)[sortid[i]].format(&msg, buffer, sizeof(buffer)))
            {
                sortlist[i] = buffer;
                fprintf(out, "\\gdef\\repgroupname%s{%s}%%\n", str.c_str(),
                        ToString::mktex(buffer).c_str());
                str = str + "i";
            }
            else
            {
                break;
            }
        }

        for (i = 0; i < ids.size(); ++i)
        {
            if ( i < (*iprm).size() )
            {
                int t = typs[i];
                char str[64];
                struct tm tm;
                time_t val;

                if ( ((*iprm)[i]).isnull )
                {
                    fprintf(out, "\\gdef\\%s{}%%\n",ToString::mktexmacro(ids[i]).c_str());
                }
                else if ( t == DbConnect::DATETIME )
                {
                    val = (long)((*iprm)[i]);
                    if ( formats[i] != "l" )
                        strftime(str, sizeof(str), sdtf.c_str(), localtime_r(&val, &tm));
                    else
                        strftime(str, sizeof(str), dtf.c_str(), localtime_r(&val, &tm));
                        fprintf(out, "\\gdef\\%s{%s}%%\n",ToString::mktexmacro(ids[i]).c_str(),ToString::mktex(str).c_str());
                }
                else if ( t == DbConnect::DATE )
                {
                    val = (long)((*iprm)[i]);
                    strftime(str, sizeof(str), df.c_str(), localtime_r(&val, &tm));
                    fprintf(out, "\\gdef\\%s{%s}%%\n",ToString::mktexmacro(ids[i]).c_str(),ToString::mktex(str).c_str());
                }
                else if ( t == DbConnect::TIME )
                {
                    val = (long)((*iprm)[i]);
                    if ( formats[i] != "l" )
                        strftime(str, sizeof(str), stf.c_str(), localtime_r(&val, &tm));
                    else
                        strftime(str, sizeof(str), tf.c_str(), localtime_r(&val, &tm));
                    fprintf(out, "\\gdef\\%s{%s}%%\n",ToString::mktexmacro(ids[i]).c_str(),ToString::mktex(str).c_str());
                }
                else if ( t == DbConnect::INTERVAL )
                {
                    val = (long)((*iprm)[i]);
                    if ( val < 0 )
                    {
                        val = -val;
                        if ( formats[i] != "l" )
                            sprintf(str, ("-" + sintf).c_str(), val / 3600, ( val % 3600 ) / 60, ( val % 60 ));
                        else
                            sprintf(str, ("-" + intf).c_str(), val / 3600, ( val % 3600 ) / 60, ( val % 60 ));
                    }

                    else
                    {
                        if ( formats[i] != "l" )
                            sprintf(str, sintf.c_str(), val / 3600, ( val % 3600 ) / 60, ( val % 60 ));
                        else
                            sprintf(str, intf.c_str(), val / 3600, ( val % 3600 ) / 60, ( val % 60 ));

                    }
                    fprintf(out, "\\gdef\\%s{%s}%%\n",ToString::mktexmacro(ids[i]).c_str(),ToString::mktex(str).c_str());
                }
                else if ( t == DbConnect::DOUBLE || t == DbConnect::FLOAT )
                {
                    std::string colformat = formats[i];
                    std::string::size_type j;

                    if ( ( j = colformat.find('%',0) != std::string::npos ))
                        colformat.insert(j, 1, '\'');

                    if ( colformat == "" ) colformat = "%'f";
                    fprintf(out, "\\gdef\\%s{%s}%%\n", ToString::mktexmacro(ids[i]).c_str(), ToString::mktex( (*iprm)[i].format(&msg, NULL, 0, colformat.c_str())).c_str());
                }
                else if ( formats[i][0] != 'x' )
                {
                    fprintf(out, "\\gdef\\%s{%s}%%\n", ToString::mktexmacro(ids[i]).c_str(), ToString::mktex( (*iprm)[i].format(&msg, NULL, 0, formats[i].c_str()), formats[i] == "tex").c_str());
                }
                else
                {
                    XmlTextTex xml;
                    xml.setXml((char *) ((*iprm)[i]));
                    fprintf(out, "\\gdef\\%s{", ToString::mktexmacro(ids[i]).c_str());
                    xml.mk_output(out);
                    fprintf(out, "}%%\n");
                }
            }
            else
            {
                msg.perror(E_NODATA, "keine Daten für id <%s> vorhanden", ids[i].c_str());
                fprintf(out, "\\gdef\\%s{keine Daten}%%\n", ToString::mktexmacro(ids[i]).c_str());
            }
        }

        fseek(fp, SEEK_SET, 0);
        while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
            fwrite(buffer, size, 1, out);

        w.clear();
        w["parent"] = reportname;
        w["before"] = 0;
        cols.setString("name,cols,parentcols,ops");
        sort.setString("num");
        rm = *(reptab->select(&cols, &w, NULL, &sort));

        for (i = 0; i < rm.size(); ++i)
        {
            CsList cols((char*) (rm[i][1]), ';');
            CsList pcols((char*) (rm[i][2]), ';');
            CsList pvals("", ';');
            CsList wops((char*) (rm[i][3]), ';');
            unsigned int n;

            for (n = 0; n < pcols.size(); ++n)
            {
                std::vector<std::string>::iterator f;

                if ((pcols[n])[0] == '#')
                    pvals.add(pcols[n].substr(1));
                else if ( (f = std::find(ids.begin(), ids.end(), "B" + pcols[n])) != ids.end())
                    pvals.add(((*iprm)[f - ids.begin()]).format());
                else
                    pvals.add("");
            }

            mk_report(db, (char*) (rm[i][0]), 1, out, langid, "", "", &cols, &pvals, &wops,
                    NULL);
        }

        if (post != NULL)
        {
            fseek(post, SEEK_SET, 0);
            while ((size = fread(buffer, 1, sizeof(buffer), post)) > 0)
                fwrite(buffer, size, 1, out);
        }
    }

    fclose(fp);
    if (pre != NULL)
        fclose(pre);
    if (post != NULL)
        fclose(post);

    db->release(query);
    db->release(reptab);

    str = getFile(rtemplate + "/trailer.tex");
    if ( str != "" ) fp = fopen(str.c_str(), "r"); else fp = NULL;

    if (fp == NULL)
    {
        msg.perror(E_NOTRAILER, "kann datei %s nicht finden", (rtemplate + "/trailer.tex").c_str());
        msg.setLang(langsave);
        return record_count;
    }

    while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
        fwrite(buffer, size, 1, out);

    fclose(fp);

    if (!subreport)
    {
        str = getFile(rtemplate + "/end.tex");
        if ( str == "" ) str = getFile("/allg/default/end.tex");
        if ( str != "" ) fp = fopen(str.c_str(), "r"); else fp = NULL;

        if ( fp == NULL )
        {
            msg.perror(E_NOEND, "kann datei %s nicht finden", (rtemplate + "/end.tex").c_str());
            msg.setLang(langsave);
            return record_count;
        }

        while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
            fwrite(buffer, size, 1, out);

        fclose(fp);
    }
    else
    {
        str = getFile(rtemplate + "/subend.tex");
        if ( str != "" ) fp = fopen(str.c_str(), "r"); else fp = NULL;

        if ( fp != NULL )
        {
            while ((size = fread(buffer, 1, sizeof(buffer), fp)) > 0)
                fwrite(buffer, size, 1, out);
            fclose(fp);
        }
    }

    msg.setLang(langsave);
    return record_count;
}

