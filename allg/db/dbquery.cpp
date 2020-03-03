#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <utils/tostring.h>
#include <utils/gettimeofday.h>

#include "dbquery.h"
#include "database.h"
#include "dbjoin.h"

DbQuery::DbQuerySingleMap DbQuery::querys;
pthread_mutex_t DbQuery::query_mutex = PTHREAD_MUTEX_INITIALIZER;

#define COL_SELECT "SELECT t0.tabnum,\
                           t0.field,\
                           t0.colid,\
                           COALESCE(NULLIF(t2.text_de,''), NULLIF(t4.text_de, ''), NULLIF(t5.text_de, ''), NULLIF(tcn.text_de,''), NULLIF(t0.colid,''), t0.field),\
                           t0.fieldtyp,\
                           t0.lang,\
                           t0.format,\
                           t1.unionnum,\
                           COALESCE(NULLIF(t4.dpytype, -1), NULLIF(tcn.dpytype,-1), NULLIF(t0.fieldtyp,-1), -1),\
                           t0.groupby,\
                           t0.cannull,\
                           COALESCE( NULLIF(t6.regexp, ''), NULLIF(t4.regexp, ''), NULLIF( t5.regexp, ''), NULLIF(rtcn.regexp, ''), NULLIF(tcn.regexp,'')),\
                           COALESCE(NULLIF(t7.text_de,''), NULLIF(t8.text_de,''), NULLIF(trtcn.text_de,''), NULLIF(ttcn.text_de,''), NULLIF(t6.regexphelp,''), NULLIF(t4.regexphelp,''), NULLIF(t5.regexphelp,''), NULLIF(rtcn.regexphelp,''), NULLIF(tcn.regexphelp,'')),\
                           COALESCE( NULLIF(t6.regexpmod,''), NULLIF(t4.regexpmod,''), NULLIF( t5.regexpmod,''), NULLIF(rtcn.regexpmod,''), NULLIF(tcn.regexpmod,'')),\
                           t0.musthaving \
                    FROM ( (((((((((mne_application.querycolumns t0 LEFT JOIN mne_application.queryname t1 ON ( t0.queryid = t1.queryid) ) \
                          LEFT JOIN mne_application.querycolnames t2 ON ( t1.schema = t2.schema AND t1.query = t2.query AND t0.colid = t2.colid) ) \
                          LEFT JOIN mne_application.querytables t3 ON ( t0.queryid = t3.queryid AND t0.tabnum = t3.tabnum) ) \
                          LEFT JOIN mne_application.tablecolnames t4 ON ( t3.tschema = t4.schema AND t3.ttab = t4.tab AND t0.field = t4.colname) ) \
                          LEFT JOIN mne_application.tablecolnames t5 ON ( t3.tschema = t5.schema AND t3.ttab = t5.tab AND t0.field || '_de' = t5.colname) ) \
                          LEFT JOIN mne_application.tableregexp t6 ON ( t4.regexp = t6.tableregexpid) ) \
                          LEFT JOIN mne_application.translate t7 ON ( t6.regexphelp = t7.id) )) \
                          LEFT JOIN mne_application.translate t8 ON ( t4.regexphelp = t8.id)) \
                          LEFT JOIN mne_application.tablecolnames tcn  ON tcn.schema = '' AND tcn.tab = ''  AND tcn.colname = t0.field\
                          LEFT JOIN mne_application.translate ttcn ON tcn.regexphelp = ttcn.id\
                          LEFT JOIN mne_application.tableregexp rtcn ON tcn.regexp = rtcn.tableregexpid\
                          LEFT JOIN  mne_application.translate trtcn ON rtcn.regexphelp = trtcn.id)\
                    WHERE  t1.schema = '%s' AND  t1.query = '%s'%s \
                    ORDER BY t1.unionnum, t0.colnum"

#define TAB_SELECT "SELECT t0.deep, t0.fcols, t0.tschema, t0.ttab, t0.tcols, t0.op, t0.typ, t0.tabnum, t1.unionnum \
                    FROM mne_application.querytables t0 \
                     LEFT JOIN mne_application.queryname t1 ON ( t0.queryid = t1.queryid) \
                    WHERE  t1.schema = '%s' AND  t1.query = '%s'%s \
                    ORDER BY t1.unionnum, t0.tabid"

#define WHERE_SELECT "SELECT t0.notoperator, t0.leftbrace, t0.lefttab, t0.leftvalue, t0.operator, t0.righttab, t0.rightvalue, t0.rightbrace, t0.booloperator, t1.unionnum \
                      FROM mne_application.querywheres t0 \
                        LEFT JOIN mne_application.queryname t1 ON ( t0.queryid = t1.queryid) \
                      WHERE  t1.schema = '%s' AND  t1.query = '%s'%s \
                      ORDER BY t1.unionnum, t0.wherecol"

DbQuerySingle::DbQuerySingle(Database *db) :
        msg("DbQuerySingle")
{
    this->db = db;
    this->dbadmin = db->getDatabase();
    this->dbadmin->p_getConnect(db->p_getConnect());
    this->cur = db->p_getCursor();
    this->errorfound = 0;
}

DbQuerySingle::~DbQuerySingle()
{
    std::map<std::string, DbTable *>::iterator i;
    for (i = tables.begin(); i != tables.end(); ++i)
        dbadmin->release(i->second);

    std::vector<DbJoin *>::iterator j;
    for (j = joins.begin(); j != joins.end(); ++j)
        dbadmin->release(*j);

    db->release(cur);
    delete this->dbadmin;
}

std::string DbQuerySingle::mk_statement(DbTable::ValueMap *w, CsList *wop, CsList *sort, CsList *params)
{
    if (w != NULL)
    {
        CsList wcol;
        CsList wval;
        DbTable::ValueMap::iterator wi;

        for (wi = w->begin(); wi != w->end(); ++wi)
        {
            wcol.add(wi->first);
            wval.add(wi->second.value);
        }
        return mk_statement(&wcol, &wval, wop, sort, params);
    }
    else
    {
        return mk_statement(NULL, NULL, wop, sort, params);
    }

}

std::string DbQuerySingle::mk_statement(CsList *wcol, CsList *wval, CsList *wop, CsList *sort, CsList *params)
{

    std::string where, from;
    std::string stmconcat, stmend;

    std::vector<std::string>::iterator vi;
    std::map<std::string,std::string>::iterator si;
    std::vector<std::string> act_groupby;

    std::string::size_type k, j, i, index;
    std::string having;
    std::string no;
    int ornull;

    if (this->errorfound) return "";
    for (j = 0; j < joins.size(); ++j)
    {
        where = joins[j]->getWhere();

        if (j == 0) laststm = "SELECT ";
        else if (unionall[j]) laststm += " UNION ALL SELECT ";
        else laststm += " UNION SELECT ";

        if (distinct[j]) laststm += " DISTINCT ";

        from = joins[j]->getFrom();
        i = 0;
        while (i < from.size() && (i = from.find("#t", i)) != std::string::npos)
        {
            k = from.find_first_of(".#", i + 1);
            if (k != std::string::npos && from[k] != '#') from.replace(i, k - i, tabnums[j * 1000 + atoi(from.substr(i + 2, k - i - 2).c_str())]);
            i++;
        }


        laststm = laststm + sel_fields[j] + "\nFROM\n" + from;

        if (where != "")
        {
            laststm = laststm + " WHERE\n  ( " + where + " ) ";
            stmconcat = " AND ";
        }
        else
        {
            stmconcat = " WHERE ";
        }
        where = "";
        having = "";

        if (wcol != NULL && wcol->size() > 0)
        {
            std::string gwconcat = "AND";
            std::string ghconcat = "AND";
            std::string wconcat;
            std::string hconcat;
            unsigned int woffset = 0;
            if (wop != NULL && (*wop)[0] == "OR")
            {
                woffset = 1;
                gwconcat = ghconcat = "OR";
            }

            DbConnect *con = dbadmin->p_getConnect();
            for (i = 0; i < wcol->size(); ++i)
            {
                std::string op = "=";
                std::string str = "";
                std::string startstr = "";
                std::string endstr = "";

                if (where != "") wconcat = gwconcat;
                if (having != "") hconcat = ghconcat;
                no = "";
                ornull = 0;

                if (wop != NULL)
                {
                    op = (*wop)[i + woffset];
                    if (op[0] == 'o')
                    {
                        op = op.substr(1);
                        if (where != "") wconcat = "OR";
                        if (having != "") hconcat = "OR";
                    }
                    if (op[0] == '^')
                    {
                        op = op.substr(1);
                        no = " NOT ";
                    }
                    if (op[0] == '0')
                    {
                        op = op.substr(1);
                        ornull = 1;
                    }
                    if ( op[0] == '(' )
                    {
                        op = op.substr(1);
                        startstr = "(";
                    }
                    if ( op[op.length() - 1] == ')' )
                    {
                        op = op.substr(0, op.length() - 1 );
                        endstr = ")";
                    }

                }

                if ((vi = std::find(sel_id.begin(), sel_id.end(), (*wcol)[i])) != sel_id.end())
                {
                    index = vi - sel_id.begin();
                    long vt = sel_typ[index];
                    if (vt == 1010 || vt == 1011) vt = DbConnect::CHAR;

                    if (wop == NULL || op == "")
                    {
                        if (vt == DbConnect::CHAR && sel_null[index] && (*wval)[i] == "")
                        {
                            str = " ( " + sel_field[j][index] + " ) IS NULL";
                        }
                        else
                        {
                            str = " ( " + sel_field[j][index] + " ) = " + con->getValue(vt, (*wval)[i]);
                        }
                    }
                    else
                    {
                        if (vt == DbConnect::CHAR && op == "like")
                        {
                            if (ornull) str = " (( lower(" + sel_field[j][index] + ") " + op + " lower(" + con->getValue(sel_typ[index], (*wval)[i]) + ") OR ( " + sel_field[j][index] + " IS NULL )) ";
                            else str = "( lower(" + sel_field[j][index] + ")) " + op + " lower(" + con->getValue(sel_typ[index], (*wval)[i]) + ")";
                        }
                        else if (vt == DbConnect::CHAR && op == "in")
                        {
                            str = " ( " + sel_field[j][index] + " ) " + op + " " + (*wval)[i];
                        }
                        else if (vt == DbConnect::CHAR && sel_null[index] && (*wval)[i] == "" && !ornull)
                        {
                            if (op == "=") str = " ( " + sel_field[j][index] + " ) IS NULL";
                            else str = " ( " + sel_field[j][index] + " ) IS NOT NULL";
                        }
                        else if ((op).find("NULL") != std::string::npos || (op).find("null") != std::string::npos)
                        {
                            str = " ( " + sel_field[j][index] + " ) " + op;
                        }
                        else
                        {
                            if (ornull) str = " (( " + sel_field[j][index] + " " + op + " " + con->getValue(sel_typ[index], (*wval)[i]) + " ) OR ( " + sel_field[j][index] + " IS NULL )) ";
                            else str = " ( " + sel_field[j][index] + " ) " + op + " " + con->getValue(sel_typ[index], (*wval)[i]);
                        }
                    }
                }
                else
                {
                    unsigned long j;
                    this->errorfound = 1;
                    index = std::string::npos;
                    msg.perror(E_SELID, "Spaltenid <%s> für Whereclause in Sicht <%s> "
                            "nicht gefunden", (*wcol)[i].c_str(), (schema + ":" + name).c_str());
                    for (j = 0; j < sel_id.size(); ++j)
                        msg.line("%s", sel_id[j].c_str());
                    this->errorfound = 1;
                }

                if (sel_musthaving[j][index]) having += " "  + hconcat + " " + startstr + no + str + endstr;
                else where += " " + wconcat + startstr  + no + str + endstr;
            }
            if (where != "") laststm += stmconcat + "( " + where + " )";
        }

        if (params != NULL && params->size() > 0)
        {
            for (index = 0; 1; index++)
            {
                char str[16];
                int count;
                sprintf(str, "#%d", (int) index);

                for ( count=0; (i = laststm.find(str)) != std::string::npos; ++count)
                laststm.replace(i, strlen(str), (*params)[index]);
                if ( count == 0 ) break;
            }
        }

        act_groupby.clear();
        for ( si = sel_groupby[j].begin(); si != sel_groupby[j].end(); si++)
        {
            if ( this->sel_cols.empty() || this->sel_cols.find( si->first) != std::string::npos)
               act_groupby.push_back(si->second);
        }
        if (act_groupby.size() != 0)
        {
            laststm += " GROUP BY " + act_groupby[0];
            for (i = 1; i < act_groupby.size(); ++i)
                laststm += ", " + act_groupby[i] + "";
        }

        if (having != "") laststm += " HAVING " + having;
    }

    if (sort != NULL && sort->size() > 0)
    {
        laststm += " ORDER BY ";
        for (i = 0; i < sort->size();)
        {
            std::string str = (std::string) ((*sort)[i]);
            std::string asc;
            if (str[0] == '!')
            {
                str.replace(0, 1, "");
                asc = "DESC";
            }
            else
            {
                asc = "ASC";
            }
            if ((vi = std::find(sel_id.begin(), sel_id.end(), str)) != sel_id.end()) laststm += "\"" + *vi + "\" " + asc;
            else
            {
                this->errorfound = 1;
                msg.perror(E_SELID, "Spaltenid <%s> zum Sortieren in Sicht <%s> "
                        "nicht gefunden", ((std::string) (*sort)[i]).c_str(), (schema + ":" + name).c_str());
            }
            ++i;
            if (i < sort->size()) laststm += ", ";

        }
    }

    if (this->errorfound) return "";
    else return laststm;

}

void DbQuerySingle::setName(std::string schema, std::string name, CsList *cols, std::string unionnum)
{

    DbCursor *ccur, *tcur = NULL, *wcur = NULL;
    DbTable *tab = NULL;
    std::string stm;
    char cmd[10240];

    DbConnect::ResultVec r;
    int deepid;
    std::string lang;
    std::vector<std::string> tabs;
    int old_unionnum, offset_unionnum, count_unionnum;
    std::map<int, int> unionnum2count;
    std::map<int, int>::iterator ui;

    DbJoin *act_join;

    this->schema = schema;
    this->name = name;

    this->errorfound = 0;

    std::map<std::string, DbTable *>::iterator i;
    for (i = tables.begin(); i != tables.end(); ++i)
        dbadmin->release(i->second);

    this->tabnums.clear();
    std::vector<DbJoin *>::iterator j;
    for (j = joins.begin(); j != joins.end(); ++j)
        dbadmin->release(*j);

    lang = msg.getLang();

    ccur = dbadmin->p_getCursor();
    tcur = dbadmin->p_getCursor();
    wcur = dbadmin->p_getCursor();

    snprintf(cmd, sizeof(cmd), COL_SELECT, schema.c_str(), name.c_str(), (unionnum != "" && unionnum != "0") ? (std::string(" AND t1.unionnum = ") + unionnum ).c_str(): "" );
    ccur->open((char*) cmd);
    snprintf(cmd, sizeof(cmd), TAB_SELECT, schema.c_str(), name.c_str(), (unionnum != "" && unionnum != "0") ? (std::string(" AND t1.unionnum = ") + unionnum ).c_str(): "" );
    tcur->open((char*) cmd);
    snprintf(cmd, sizeof(cmd), WHERE_SELECT, schema.c_str(), name.c_str(), (unionnum != "" && unionnum != "0") ? (std::string(" AND t1.unionnum = ") + unionnum ).c_str(): "" );
    wcur->open((char*) cmd);

    try
    {
        r = tcur->next();

        old_unionnum = -1;
        count_unionnum = -1;
        deepid = 0;
        while (!tcur->eof())
        {
            if ((long) r[8] != old_unionnum)
            {
                char str[128];

                if (old_unionnum >= 0)
                {
                    joins.push_back(act_join);
                    unionnum2count[old_unionnum] = joins.size() - 1;
                }
                old_unionnum = (long) r[8];
                sprintf(str, "%d", old_unionnum);
                count_unionnum++;
                offset_unionnum = count_unionnum * 1000;
                act_join = dbadmin->p_getJoin();
                stm = "SELECT "
                        "    unionall, selectdistinct "
                        "FROM "
                        "    " + db->getApplschema() + ".queryname "
                        "WHERE "
                        "      schema = '" + schema + "' AND query = '" + name + "' AND unionnum = " + str;

                db->p_getConnect()->execute((char *) stm.c_str());
                if (db->p_getConnect()->have_result())
                {
                    DbConnect::ResultVec r;

                    r = *(db->p_getConnect()->p_get_first_result());
                    unionall.push_back((long) (r[0]));
                    distinct.push_back((long) (r[1]));
                }
                else
                {
                    unionall.push_back(1);
                    distinct.push_back(0);
                }
            }

            if ((long) r[0] < deepid) deepid = (long) r[0];

            if ((long) r[0] == deepid)
            {
                tab = dbadmin->p_getTable((char*) r[2], (char*) r[3]);

                if (tab->p_getColumns()->size() == 0)
                {
                    msg.pwarning(W_TABLOST, "Tabelle %s.%s existiert nicht mehr", (char*) r[2], (char*) r[3]);
                    dbadmin->release(tab);
                }
                else
                {
                    act_join->back(deepid);
                    tabnums[offset_unionnum + ((long) r[7])] = act_join->add_table((std::string) r[2] + "." + (std::string) r[3], (char *) r[1], (char*) r[4], (char*) r[5], (long) r[6]);
                    tables[ToString::convert((long) count_unionnum) + "_" + tabnums[offset_unionnum + ((long) r[7])]] = tab;
                    deepid++;
                }
            }

            r.clear();
            r = tcur->next();
            if (tcur->eof())
            {
                joins.push_back(act_join);
                unionnum2count[old_unionnum] = joins.size() - 1;
            }
        }

        r.clear();

        std::vector<std::string> act_sel_field;
        std::map<std::string,std::string> act_groupby;
        std::vector<long> act_musthaving;
        int first, n;
        int unionnumid;

        r = ccur->next();
        old_unionnum = -1;
        count_unionnum = -1;
        first = 1;

        while (!ccur->eof())
        {
            std::string field;

            if ((long) r[7] != old_unionnum)
            {
                if (old_unionnum != -1)
                {
                    if (sel_musthaving.size() > 0 && sel_musthaving[0].size() != act_musthaving.size())
                    {
                        msg.perror(E_SELFIELDS, "Anzahl der Selektfelder ist nicht gleich bei unionnum <%d>", old_unionnum);
                        throw 1;
                    }
                    sel_field.push_back(act_sel_field);
                    sel_groupby.push_back(act_groupby);
                    sel_musthaving.push_back(act_musthaving);
                    first = 0;
                }

                old_unionnum = (long) r[7];
                if ((ui = unionnum2count.find(old_unionnum)) != unionnum2count.end())
                {
                    count_unionnum = ui->second;
                }
                else
                {
                    msg.perror(E_UNIONNUM, "Unionnum ist unbekannt <%d>", old_unionnum);
                    throw 1;
                }

                offset_unionnum = count_unionnum * 1000;
                act_sel_field.clear();
                act_musthaving.clear();
                act_groupby.clear();

                n = 0;
            }

            unionnumid = offset_unionnum + (long) r[0];

            if (tabnums.find(unionnumid) != tabnums.end() || (long) r[0] == -1)
            {
                if (first)
                {
                    sel_id.push_back((std::string) r[2]);
                    sel_format.push_back((std::string) r[6]);
                    sel_null.push_back((long) r[10]);
                    sel_regexp.push_back((std::string) r[11]);
                    sel_regexphelp.push_back((std::string) r[12]);
                    sel_regexpmod.push_back((std::string) r[13]);
                    sel_typ.push_back((long)r[8]);
                    sel_name.push_back((char *) r[3]);
                }

                if (sel_typ[n] == -1)
                    sel_typ[n] = r[8];

                act_musthaving.push_back((long) (r[14]));

                if ((long) (r[9]))
                {
                    if (tabnums[unionnumid] != "")
                    {
                        std::string s = tabnums[unionnumid] + "." + (std::string) (r[1]);
                        if ((long) r[5] != 0) s = s + "_" + lang;
                        act_groupby.insert(std::pair<std::string,std::string>( (std::string)r[2],s));
                    }
                    else
                    {
                        act_groupby.insert(std::pair<std::string,std::string>( (std::string)r[2],(std::string)r[2]));
                    }
                }

                if ((long) r[0] >= 0)
                {
                    if (((char *) r[1])[0] == '(')
                    {
                        std::string::size_type i;

                        field = (char*) r[1];
                        while ((i = field.find("t.")) != std::string::npos)
                            field.replace(i++, 1, tabnums[unionnumid]);
                    }
                    else
                    {
                        if ((long) r[5] != 0) field = tabnums[unionnumid] + ".\"" + (char*) r[1] + "_" + lang + "\"";
                        else field = tabnums[unionnumid] + ".\"" + (char*) r[1] + "\"";

                    }
                }
                else if ((long) r[5] != 0)
                {
                    std::string str = (std::string) r[1];
                    if (str[0] == '\'' && str[str.size() - 1] == '\'') field = db->p_getConnect()->getValue(DbConnect::CHAR, msg.get(str.substr(1, str.size() - 2)));
                    else
                    {
                        msg.pwarning(W_NOTSTRING, "<%s> kann nicht übersetzt werden", str.c_str());
                        field = db->p_getConnect()->getValue(DbConnect::CHAR, str);
                    }
                }
                else
                {
                    std::string::size_type i, j, k;
                    field = (std::string) r[1];
                    i = k = 0;
                    while ((i = field.find('\'', i)) != std::string::npos)
                    {
                        if (k++ == 0) field.replace(i++, 1, "E'");
                        else if (i == 0 || field[i - 1] != '\\') k = 0;
                        i++;
                    }

                    i = 0;
                    while ((i = field.find("#mne_langid#", i)) != std::string::npos)
                        field.replace(i, 12, lang);

                    i = 0;
                    while (i < field.size() && (i = field.find("#mne_lang#", i)) != std::string::npos)
                    {
                        if ((j = field.find_first_of("#'\"", i + 11)) != std::string::npos)
                        {
                            std::string::size_type k = j;
                            if (field[j] != '#') j--;
                            field.replace(i, j - i + 1, msg.get(field.substr(i + 10, k - i - 10)));
                        }
                        i++;
                    }

                    i = 0;
                    while (i < field.size() && (i = field.find("#", i)) != std::string::npos)
                    {
                        j = field.find_first_of(".#", i + 1);
                        if (j != std::string::npos && field[j] != '#') field.replace(i, j - i, tabnums[offset_unionnum + atoi(field.substr(i + 1, j - i - 1).c_str())]);
                        i++;
                    }
                }

                act_sel_field.push_back(field);

            }
            else
            {
                msg.perror(E_TABID, "Kann Table mit Id %d nicht finden", unionnumid);
                throw 1;
            }
            n++;

            r.clear();
            r = ccur->next();
            if (ccur->eof())
            {
                if (sel_fields.size() > 0 && sel_field[0].size() != act_sel_field.size())
                {
                    msg.perror(E_SELFIELDS, "Anzahl der Selektfelder ist nicht gleich bei unionnum <%d>", old_unionnum);
                    throw 1;
                }
                sel_field.push_back(act_sel_field);
                sel_groupby.push_back(act_groupby);
                sel_musthaving.push_back(act_musthaving);
            }
        }

        setCols(cols);

        r.clear();
        r = wcur->next();
        count_unionnum = -1;
        old_unionnum = -1;
        while (!wcur->eof())
        {
            std::map<int, std::string>::iterator i;
            std::string t1, t2;
            std::string v1, v2;
            std::string op, boolop;
            long notop, lb, rb;

            if (old_unionnum != (long) r[9])
            {
                old_unionnum = (long) r[9];
                if (unionnum != "" && unionnum != "0")
                {
                    count_unionnum = 0;
                    offset_unionnum = 0;
                    act_join = joins[0];
                }
                else
                {
                    if ((ui = unionnum2count.find(old_unionnum)) != unionnum2count.end())
                    {
                        count_unionnum = ui->second;
                    }
                    else
                    {
                        msg.perror(E_UNIONNUM, "Unionnum ist unbekannt <%d>", old_unionnum);
                        throw 1;
                    }
                    offset_unionnum = count_unionnum * 1000;
                    act_join = joins[count_unionnum];
                }
            }

            t1 = t2 = "";

            if ((std::string) r[2] != "" && (i = tabnums.find(offset_unionnum + atoi((char*) r[2]))) != tabnums.end()) t1 = i->second;

            if ((std::string) r[5] != "" && (i = tabnums.find(offset_unionnum + atoi((char*) r[5]))) != tabnums.end()) t2 = i->second;

            v1 = ((char*) r[3]);
            v2 = ((char*) r[6]);
            notop = (long) r[0];
            lb = (long) r[1];
            op = (char*) r[4];
            rb = (long) r[7];
            boolop = (char*) r[8];

            if (t1 != "" && t2 == "")
            {
                DbTable *t = tables[ToString::convert((long) count_unionnum) + "_" + t1];
                DbTable::Column c = t->getColumn(v1);
                c.value = v2;
                c.can_null = 0;
                v2 = t->getValue(&c);
            }

            if (t1 == "" && t2 != "")
            {
                DbTable *t = tables[ToString::convert((long) count_unionnum) + "_" + t2];
                DbTable::Column c = t->getColumn(v2);
                c.value = v1;
                c.can_null = 0;
                v1 = t->getValue(&c);
            }

            if (t1 == "" && t2 == "")
            {
                std::string::size_type i, j;
                i = 0;
                while (i < v1.size() && (i = v1.find("#", i)) != std::string::npos)
                {
                    j = v1.find_first_of(".#", i + 1);
                    if (j != std::string::npos && v1[j] != '#') v1.replace(i, j - i, tabnums[offset_unionnum + atoi(v1.substr(i + 1, j - i - 1).c_str())]);
                    i++;
                }

                i = 0;
                while (i < v2.size() && (i = v2.find("#", i)) != std::string::npos)
                {
                    j = v2.find_first_of(".#", i + 1);
                    if (j != std::string::npos && v2[j] != '#') v2.replace(i, j - i, tabnums[offset_unionnum + atoi(v2.substr(i + 1, j - i - 1).c_str())]);
                    i++;
                }

            }

            r.clear();
            r = wcur->next();
            if (wcur->eof() || (long) r[9] != old_unionnum) boolop = "";

            if (v1 == "" && t1 == "" && v2 == "" && t2 == "") op = "";

            act_join->add_where(notop, lb, t1, v1, op, t2, v2, rb, boolop);
        }

        r.clear();

    }
    catch( int e )
    {
        this->errorfound = e;
    }

    tcur->close();
    ccur->close();
    wcur->close();

    dbadmin->release(ccur);
    dbadmin->release(tcur);
    dbadmin->release(wcur);

}

void DbQuerySingle::setCols(CsList *cols)
{
    std::string act_sel_fields;
    std::string::size_type sel_poscount,i,n;

    this->sel_fields.clear();
    this->sel_pos.clear();
    this->sel_cols.clear();
    if ( cols != NULL ) this->sel_cols = *cols;

    for ( n = 0, sel_poscount=0; n< this->sel_field.size(); n++)
    {
        act_sel_fields.clear();
        for ( i = 0; i<this->sel_id.size(); i++)
        {
            if (this->sel_id[i][0] != '-' && ( cols == NULL || cols->empty() || cols->find(this->sel_id[i]) != std::string::npos))
            {
                if (act_sel_fields != "") act_sel_fields += ",\n";
                act_sel_fields += this->sel_field[n][i] + " AS " + sel_id[i];
                if ( n == 0 ) sel_pos[sel_id[i]] = sel_poscount++;
            }
        }
        this->sel_fields.push_back(act_sel_fields);
    }
}

void DbQuerySingle::open(CsList *wcol, CsList *wval, CsList *wop, CsList *sort, CsList *params)
{
    cur->open((char *) mk_statement(wcol, wval, wop, sort, params).c_str());
}

DbConnect::ResultMat * DbQuerySingle::select(CsList *wcol, CsList *wval, CsList *wop, CsList *sort, CsList *params)
{
    db->p_getConnect()->execute((char *) mk_statement(wcol, wval, wop, sort, params).c_str());
    return db->p_getConnect()->p_get_result();

}
void DbQuerySingle::open(DbTable::ValueMap *w, CsList *wop, CsList *sort, CsList *params)
{
    cur->open((char *) mk_statement(w, wop, sort, params).c_str());
}

DbConnect::ResultMat * DbQuerySingle::select(DbTable::ValueMap *w, CsList *wop, CsList *sort, CsList *params)
{
    db->p_getConnect()->execute((char *) mk_statement(w, wop, sort, params).c_str());
    return db->p_getConnect()->p_get_result();
}

void DbQuerySingle::start_cols()
{
    act_sel_name = sel_name.begin();
    act_sel_id = sel_id.begin();
    act_sel_typ = sel_typ.begin();
    act_sel_format = sel_format.begin();
    act_sel_regexp = sel_regexp.begin();
    act_sel_regexphelp = sel_regexphelp.begin();
    act_sel_regexpmod = sel_regexpmod.begin();
}

int DbQuerySingle::getCols(std::string *id, std::string *name, long *typ, std::string *format, std::string *regexp, std::string *regexphelp, std::string *regexpmod)
{

    if (act_sel_name == sel_name.end() || act_sel_id == sel_id.end() || act_sel_format == sel_format.end() || act_sel_typ == sel_typ.end() || act_sel_regexp == sel_regexp.end() || act_sel_regexphelp == sel_regexphelp.end() || act_sel_regexpmod == sel_regexpmod.end()) return 0;

    if (id != NULL) *id = *act_sel_id++;
    else act_sel_id++;

    if (name != NULL) *name = *act_sel_name++;
    else act_sel_name++;

    if (typ != NULL) *typ = *act_sel_typ++;
    else act_sel_typ++;

    if (format != NULL) *format = *act_sel_format++;
    else act_sel_format++;

    if (regexp != NULL) *regexp = *act_sel_regexp++;
    else act_sel_regexp++;

    if (regexphelp != NULL) *regexphelp = *act_sel_regexphelp++;
    else act_sel_regexphelp++;

    if (regexpmod != NULL) *regexpmod = *act_sel_regexpmod++;
    else act_sel_regexpmod++;

    return 1;

}

void DbQuery::setName(std::string schema, std::string name, CsList *cols, std::string unionnum)
{

    DbQuerySingleMap::iterator i;
    std::string id = schema + ":" + name + ":" + unionnum + ":" + std::to_string(pthread_self());

    pthread_mutex_lock(&query_mutex);

    if ( ( i = this->querys.find(id))  != this->querys.end() )
    {
        this->act_query = i->second;
        this->act_query->setCols(cols);
        pthread_mutex_unlock(&query_mutex);
    }
    else
    {
        this->act_query = new DbQuerySingle(this->db);
        this->querys[id] = this->act_query;
        pthread_mutex_unlock(&query_mutex);
        this->act_query->setName(schema, name, cols, unionnum);
    }

}
