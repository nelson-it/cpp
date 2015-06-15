#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <crypt/base64.h>
#include <utils/tostring.h>
#include <xml/xmltext_html.h>

#include "dbtable.h"
#include "dbcursor.h"

#include "dbhttputils_table.h"

DbHttpUtilsTable::DbHttpUtilsTable(DbHttp *h, int noadd ) :
DbHttpProvider(h), msg("DbHttpUtilsTable")
{
    subprovider["insert.xml"] = &DbHttpUtilsTable::insert_xml;
    subprovider["modify.xml"] = &DbHttpUtilsTable::modify_xml;
    subprovider["delete.xml"] = &DbHttpUtilsTable::delete_xml;
    subprovider["data.xml"] = &DbHttpUtilsTable::data_xml;

    subprovider["modify.html"] = &DbHttpUtilsTable::modify_html;
    subprovider["file.dat"] = &DbHttpUtilsTable::file_dat;

    if ( noadd == 0 )
        h->add_provider(this);
}

DbHttpUtilsTable::~DbHttpUtilsTable()
{
}

int DbHttpUtilsTable::request(Database *db, HttpHeader *h)
{

    SubProviderMap::iterator i;

    if ( (i = subprovider.find(h->filename)) != subprovider.end() )
    {
        h->status = 200;
        (this->*(i->second))(db, h);

        if (h->vars["sqlend"] != "")
            db->p_getConnect()->end();

        return 1;
    }
    else
    {
        file_dat(db, h);
        if ( h->status == 200 )
            return 1;
    }

    return 0;

}



void DbHttpUtilsTable::mk_selectcolumns(Database *db, HttpHeader *h,
        DbTable *tab, DbTable::ColumnVec &vals, unsigned int &anzahl_cols,
        DbTable::ValueMap &where, CsList &wop)
{
    DbTable::ColumnMap::iterator ci, ce;
    CsList cols(h->vars["cols"]);
    CsList shows(h->vars["showcols"]);

    if (cols.empty() )
    {
        ci = tab->p_getColumns()->begin();
        ce = tab->p_getColumns()->end();

        for (; ci != ce; ++ci)
        {
            vals.push_back(ci->second);
            if (h->vars.exists(ci->first + "Input.old") )
                where[ci->first] = h->vars[ci->first + "Input.old"];
        }
        anzahl_cols = vals.size();
    }
    else
    {
        CsList::Elements::iterator e;
        std::string::size_type pos;
        ce = tab->p_getColumns()->end();

        e = cols.begin();
        while (e != cols.end() )
        {
            if ( (pos = e->find("#mne_langid#")) != std::string::npos )
            {
                e->replace(pos, 12, msg.getLang().c_str());
            }
            if ( (ci = tab->p_getColumns()->find(*e)) != ce)
            {
                vals.push_back(ci->second);
            }
            else
            {
                msg.perror(E_WRONG_COLUMN, "Spalte <%s> unbekannt", (*e).c_str());
            }
            ++e;
        }

        anzahl_cols = vals.size();

        e = shows.begin();
        while (e != shows.end() )
        {
            if ( (pos = e->find("#mne_langid#")) != std::string::npos )
            {
                e->replace(pos, 12, msg.getLang().c_str());
            }
            if ( (ci = tab->p_getColumns()->find(*e)) != ce)
            {
                vals.push_back(ci->second);
            }
            else
            {
                msg.perror(E_WRONG_COLUMN, "Spalte <%s> unbekannt", (*e).c_str());
            }
            ++e;
        }

        ci = tab->p_getColumns()->begin();

        for (; ci != ce; ++ci)
        {
            if (h->vars.exists(ci->first + "Input.old") )
                where[ci->first] = h->vars[ci->first + "Input.old"];
        }
    }
    if (h->vars["wcol"] != "")
    {
        unsigned int i;
        std::string::size_type n;
        DbTable::ValueMap::iterator ii;

        CsList wcol(h->vars["wcol"]);
        CsList wval(h->vars["wval"]);

        for ( i = 0; i<wcol.size() && i < wval.size(); ++i )
            where[wcol[i]] = wval[i];

        for ( ; i < wcol.size(); ++i )
            where[wcol[i]] = "";

        CsList op(h->vars["wop"]);
        for ( ii = where.begin(); ii != where.end(); ++ii )
        {
            if ( ( n = wcol.find(ii->first)) != std::string::npos ) wop.add(op[n]);
            else wop.add("=");
        }


    }
}

void DbHttpUtilsTable::data_xml(Database *db, HttpHeader *h)
{
    std::string schema;
    std::string table;
    unsigned int i;

    DbTable *tab;
    DbConnect::ResultMat *r;
    DbTable::ValueMap where;
    DbTable::ColumnVec vals;
    unsigned int anzahl_cols;
    std::string lang;
    std::string::size_type pos;
    std::vector<std::string>::iterator si;

    h->status = 200;
    h->content_type = "text/xml";

    schema = h->vars["schema"];
    table = h->vars["table"];

    tab = db->p_getTable(schema, table);

    CsList wop("");
    CsList cols(h->vars["cols"]);
    CsList sorts(h->vars["scols"]);
    CsList shows(h->vars["showcols"]);

    mk_selectcolumns(db, h, tab, vals, anzahl_cols, where, wop);

    fprintf(h->content,
            "<?xml version=\"1.0\" encoding=\"%s\"?><result><head>",
            h->charset.c_str());

    lang = msg.getLang();
    if ( anzahl_cols > 0 && vals[0].text.find(lang) == vals[0].text.end() )
        lang = "de";
    for (i=0; i < anzahl_cols; ++i)
    {
        fprintf(h->content, "<d><id>%s</id><typ>%ld</typ><name>%s</name><regexp><reg>%s</reg><help>%s</help><mod>%s</mod></regexp></d>\n",
                vals[i].name.c_str(), ( vals[i].dpytyp == -1 ) ? vals[i].typ : vals[i].dpytyp, vals[i].text[lang].c_str(), vals[i].regexp.c_str(), vals[i].regexphelp[lang].c_str(), vals[i].regexpmod.c_str());
    }

    for ( si=sorts.begin(); si != sorts.end(); ++si)
    {
        if ( (pos = si->find("#mne_langid#")) != std::string::npos )
        {
            si->replace(pos, 12, lang.c_str());
        }
    }

    fprintf(h->content, "</head>");

   if ( ( h->vars["no_vals"] == "" || h->vars["no_vals"] == "false" )  && h->error_found == 0 )
    {
        DbConnect::ResultMat::iterator rm;
        DbConnect::ResultVec::iterator rv, re;

        r = tab->select(&vals, &where, &wop, &sorts, ( h->vars["distinct"] != "" && h->vars["distinct"] != "0" && h->vars["distinct"] != "false" ) );
        if (h->vars["sqlend"] != "")
            db->p_getConnect()->end();

        fprintf(h->content, "<body>");
        for (rm = r->begin(); rm != r->end(); ++rm, ++i)
        {
            fprintf(h->content, "<r>");
            rv = (*rm).begin();
            re = (*rm).end();
            for (i=0; rv != re; ++rv,++i)
            {
                if ( vals[i].dpytyp == DbConnect::BINARY )
                    fprintf(h->content, "<v>binary</v>\n");
                else
                    fprintf(h->content, "<v>%s</v>\n", ToString::mkxml( rv->format(&msg)).c_str());
            }
            fprintf(h->content, "</r>");
        }
        fprintf(h->content, "</body>");
    }

    if (h->vars["lastquery"] != "" )
    {
        msg.pmessage(0,"Letze Abfrage:");
        msg.ignore_lang = 1;
        msg.line("%s", tab->getLaststatement().c_str());
    }

    db->release(tab);
    return;
}
void DbHttpUtilsTable::file_dat(Database *db, HttpHeader *h)
{
    std::string schema;
    std::string table;

    DbTable *tab;
    DbConnect::ResultMat *r;
    DbTable::ValueMap where;
    DbTable::ColumnVec vals;
    unsigned int anzahl_cols;

    h->status = 200;
    h->content_type = "text/html";

    schema = h->vars["schema"];
    table = h->vars["table"];

    tab = db->p_getTable(schema, table);

    CsList wop("");
    CsList cols(h->vars["cols"]);
    if ( cols.size() != 1 )
    {
        fprintf(h->content, "%s", msg.get("Es ist mehr als eine Spalte angebegen").c_str());
        db->release(tab);
        if (h->vars["sqlend"] != "")
            db->p_getConnect()->end();
        return;
    }
    cols.add(cols[0] + "type");
    (*h->vars.p_getVars())["cols"] = cols.getString();

    mk_selectcolumns(db, h, tab, vals, anzahl_cols, where, wop);

    r = tab->select(&vals, &where, &wop, NULL, ( h->vars["distinct"] != "" && h->vars["distinct"] != "0" && h->vars["distinct"] != "false" ) );
    if ( r->size() > 1 )
    {
        fprintf(h->content, "%s", msg.get("Abfrage hat mehr als ein Ergebnis").c_str());
    }
    else if ( r->size() == 0 )
    {
        h->status = 404;
        return;
    }
    else
    {
        char str[10240];
        int rlen;
        snprintf(str, sizeof(str), "Content-Disposition: attachment; filename=\"%s\"", h->vars.url_decode(h->filename).c_str());
        str[sizeof(str) -1] = '\0';
        h->extra_header.push_back(str);

        h->content_type = (char*)(((*r)[0])[1]);

        CryptBase64 base64;
        unsigned char *out = (unsigned char*) new char[(*r)[0][0].length];
        rlen = base64.decode((unsigned char*)((char *)(*r)[0][0]), out, strlen(((char *)(*r)[0][0])));
        fwrite(out, 1, rlen, h->content);
        delete out;
    }

    db->release(tab);

    if (h->vars["sqlend"] != "")
        db->p_getConnect()->end();
}

void DbHttpUtilsTable::insert_xml(Database *db, HttpHeader *h)
{
    std::string schema;
    std::string table;
    std::string str;
    std::string val;

    DbTable::ColumnMap::iterator ci;
    DbTable::ValueMap vals;
    DbTable::ValueMap orig_vals;
    DbTable::ValueMap where;
    DbTable::ValueMap::iterator wi;
    DbTable *act_table;

    schema = h->vars["schema"];
    table = h->vars["table"];

    h->status = 200;
    h->content_type = "text/xml";

    fprintf(h->content,
            "<?xml version=\"1.0\" encoding=\"%s\"?><result>",
            h->charset.c_str());

    act_table = db->p_getTable(schema, table);

    ci = act_table->p_getColumns()->begin();
    for (; ci != act_table->p_getColumns()->end(); ++ci)
    {
        str = ci->first + "Input";
        if (h->vars.exists(str) )
        {
            val = h->vars[str];

            if (ci->second.typ == DbConnect::CHAR)
            {
                if (val == "################")
                    orig_vals[ci->first] = vals[ci->first] = db->p_getConnect()->mk_index();
                else
                {
                    orig_vals[ci->first] = val;
                    vals[ci->first] = val;
                }
            }
            else
                orig_vals[ci->first] = vals[ci->first] = val;

            where[ci->first] = vals[ci->first];
        }
    }

    if ( act_table->insert(&vals) == 0 )
    {
        DbTable::ValueMap::iterator i;
        fprintf(h->content, "<body>");
        for ( i=orig_vals.begin(); i != orig_vals.end(); ++i )
            fprintf(h->content, "<%s>%s</%s>", ToString::mkxml(i->first).c_str(), ToString::mkxml(i->second.value).c_str(), ToString::mkxml(i->first).c_str());
        fprintf(h->content, "</body>");
    }
    else
        fprintf(h->content, "<body>error</body>");

    db->release(act_table);
    return;
}

void DbHttpUtilsTable::modify(Database *db, HttpHeader *h)
{
    std::string schema;
    std::string table;
    std::string obj;
    std::string str;
    std::string val, v;
    std::string prefix;
    std::string root;

    DbConnect::ResultMat *r;
    DbConnect::ResultMat::iterator ri;
    DbConnect::ResultVec::iterator rv;

    DbTable::ColumnMap::iterator ci;
    DbTable::ValueMap orig_vals;
    DbTable::ValueMap vals;
    DbTable::ValueMap where;
    DbTable::ValueMap where_new;
    DbTable *act_table;

    h->status = 200;

    schema = h->vars["schema"];
    table = h->vars["table"];


    act_table = db->p_getTable(schema, table);

    ci = act_table->p_getColumns()->begin();

    for (; ci != act_table->p_getColumns()->end(); ++ci)
    {
        str = ci->first + "Input";
        if (h->vars.exists(str) )
        {
            v = val = h->vars[str];
            if (ci->second.typ == DbConnect::CHAR)
            {
                if (val == "################")
                    orig_vals[ci->first] = vals[ci->first] = v = db->p_getConnect()->mk_index();
                else if ( val.find("##########") == 0 && val.size() > 10 && val[10] != '#')
                {
                    orig_vals[ci->first + "type"] = vals[ci->first + "type"] = std::string(val, 10);
                    orig_vals[ci->first] = vals[ci->first] = h->vars.data(str);
                }
                else
                {
                    orig_vals[ci->first] = val;
                    vals[ci->first] = val;
                    ToString::mascarade(v);
                }
            }
            else
                orig_vals[ci->first] = vals[ci->first] = val;

            where_new[ci->first] = vals[ci->first];
        }

        if (h->vars.exists(str + ".old") )
        {
            val = h->vars[str + ".old"];
            where[ci->first] = val;
        }
    }

    if ( where.size() == 0 && h->vars["modifyall"] != "1" )
    {
        msg.perror(E_MOD, "Modifizieren der gesammten Tabelle nicht gestattet");
        if ( h->content_type == "text/xml" )
        {
            fprintf(h->content,
                    "<?xml version=\"1.0\" encoding=\"%s\"?><result>",
                    h->charset.c_str());
            fprintf(h->content, "<body>error</body>");
        }
        else if ( h->content_type == "text/html" )
        {
            if ( h->vars["script"] != "" )
            {
                fprintf(h->content,"<script type=\"text/javascript\">\n");
                fprintf(h->content,"<!--\n");
                fprintf(h->content,"%s\n", h->vars["script"].c_str());
                fprintf(h->content,"//-->\n");
                fprintf(h->content,"</script>\n");
            }
        }
    }
    else if ( act_table->modify(&vals, &where) == 0 )
    {
        if (h->vars["modifyinsert"] != "")
        {
            r = act_table->select(&vals, &where_new);
            if (r->empty() )
                act_table->insert(&vals);
        }

        if ( h->content_type == "text/xml" )
        {
            DbTable::ValueMap::iterator i;
            fprintf(h->content,
                    "<?xml version=\"1.0\" encoding=\"%s\"?><result>",
                    h->charset.c_str());
            fprintf(h->content, "<body>");
            for ( i=orig_vals.begin(); i != orig_vals.end(); ++i )
                fprintf(h->content, "<%s>%s</%s>", ToString::mkxml(i->first).c_str(), ToString::mkxml(i->second.value).c_str(), ToString::mkxml(i->first).c_str());
            fprintf(h->content, "</body>");
        }
        else if ( h->content_type == "text/html")
        {
            if ( h->vars["script"] != "" )
            {
                fprintf(h->content,"<script type=\"text/javascript\">\n");
                fprintf(h->content,"<!--\n");
                fprintf(h->content,"%s\n", h->vars["script"].c_str());
                fprintf(h->content,"//-->\n");
                fprintf(h->content,"</script>\n");
            }
            fprintf(h->content,"ok");
        }
    }
    else
    {
        if ( h->content_type == "text/xml" )
        {
            fprintf(h->content,
                    "<?xml version=\"1.0\" encoding=\"%s\"?><result>",
                    h->charset.c_str());
            fprintf(h->content, "<body>error</body>");
        }
        else if ( h->content_type == "text/html" )
        {
            if ( h->vars["script"] != "" )
            {
                fprintf(h->content,"<script type=\"text/javascript\">\n");
                fprintf(h->content,"<!--\n");
                fprintf(h->content,"%s\n", h->vars["script"].c_str());
                fprintf(h->content,"//-->\n");
                fprintf(h->content,"</script>\n");
            }
            fprintf(h->content, "error");
        }

    }

    db->release(act_table);
    return;
}

void DbHttpUtilsTable::modify_html(Database *db, HttpHeader *h)
{
    h->content_type = "text/html";
    modify(db, h);
}

void DbHttpUtilsTable::modify_xml(Database *db, HttpHeader *h)
{
    h->content_type = "text/xml";
    modify(db, h);
}

void DbHttpUtilsTable::delete_xml(Database *db, HttpHeader *h)
{
    std::string schema;
    std::string table;
    std::string str;

    DbTable::ColumnMap::iterator ci;
    DbTable::ValueMap vals;
    DbTable::ValueMap where;
    DbTable *act_table;

    h->status = 200;
    h->content_type = "text/xml";

    fprintf(h->content,
            "<?xml version=\"1.0\" encoding=\"%s\"?><result>",
            h->charset.c_str());

    schema = h->vars["schema"];
    table = h->vars["table"];

    act_table = db->p_getTable(schema, table);

    ci = act_table->p_getColumns()->begin();

    for (; ci != act_table->p_getColumns()->end(); ++ci)
    {
        str = ci->first + "Input";
        vals[ci->first] = h->vars[str];
        if (h->vars.exists(str + ".old") )
            where[ci->first] = h->vars[str + ".old"];
    }

    if ( where.size() == 0 )
    {
        msg.perror(E_DEL, "Löschen der gesammten Tabelle nicht gestattet");
        fprintf(h->content, "<body>error</body>");
    }
    else if ( act_table->del(&where) == 0 )
        fprintf(h->content, "<body>ok</body>");
    else
        fprintf(h->content, "<body>error</body>");

    db->release(act_table);

    return;
}

