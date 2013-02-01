#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <utils/tostring.h>

#include "dbquery.h"
#include "database.h"
#include "dbjoin.h"

DbQuery::DbQuery(Database *db) :
    msg("DbQuery")
{
    this->db = db;
    this->dbadmin = db->getDatabase();
    this->dbadmin->p_getConnect(db->p_getConnect());
    this->cur = db->p_getCursor();
}

DbQuery::~DbQuery()
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

std::string
DbQuery::mk_statement(DbTable::ValueMap *w, CsList *wop, CsList *sort,
        CsList *params)
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

std::string
DbQuery::mk_statement(CsList *wcol, CsList *wval, CsList *wop, CsList *sort,
        CsList *params)
{

    std::string where,from;
    std::string stmconcat,stmend;
    std::vector<std::string>::iterator vi;
    std::string::size_type k, j, i, index;
    std::string having;
    std::string no;
    int ornull;

    if ( this->errorfound ) return "";
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
            if (k != std::string::npos && from[k] != '#')
                from.replace( i, k - i, tabnums[j * 1000 + atoi(from.substr(i + 2, k - i - 2).c_str())]);
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

                if ( where != "" ) wconcat = gwconcat;
                if ( having != "" ) hconcat = ghconcat;
                no = "";
                ornull = 0;

                if ( wop != NULL )
                {
                    op = (*wop)[i + woffset];
                    if (op[0] == 'o')
                    {
                        op = op.substr(1);
                        if ( where != "" ) wconcat = "OR";
                        if ( having != "" ) hconcat = "OR";
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
                }

                if ((vi = std::find(sel_id.begin(), sel_id.end(), (*wcol)[i])) != sel_id.end())
                {
                    index = vi - sel_id.begin();
                    long vt = sel_typ[index];
                    if ( vt == 1010 || vt == 1011 )
                        vt = DbConnect::CHAR;

                    if (wop == NULL || op == "")
                    {
                        if (vt == DbConnect::CHAR  && sel_null[index] && (*wval)[i] == "")
                        {
                            str =  " ( "  + sel_field[j][index] + " ) IS NULL";
                        }
                        else
                        {
                            str = " ( " + sel_field[j][index] + " ) = "  + con->getValue(vt, (*wval)[i]);
                        }
                    }
                    else
                    {
                        if ( vt == DbConnect::CHAR && op == "like")
                        {
                            if ( ornull )
                                 str =  " (( lower(" + sel_field[j][index] + ") " + op + " lower(" + con->getValue(sel_typ[index], (*wval)[i]) + ") OR ( " + sel_field[j][index] + " IS NULL )) ";
                            else
                                str =  "( lower(" + sel_field[j][index] + ")) " + op + " lower(" + con->getValue(sel_typ[index], (*wval)[i]) + ")";
                        }
                        else if ( vt == DbConnect::CHAR && op == "in")
                        {
                            str = " ( " + sel_field[j][index] + " ) " + op + " " + (*wval)[i];
                        }
                        else if ( vt == DbConnect::CHAR  && sel_null[index] && (*wval)[i] == "" )
                        {
                            if (op == "=") str =  " ( " + sel_field[j][index] + " ) IS NULL";
                            else str =  " ( " + sel_field[j][index] + " ) IS NOT NULL";
                        }
                        else if ((op).find("NULL") != std::string::npos || (op).find("null") != std::string::npos )
                        {
                                str = " ( "  + sel_field[j][index] + " ) " + op;
                        }
                        else
                        {
                            if ( ornull )
                                str =  " (( "  + sel_field[j][index] + " " + op + " " + con->getValue(sel_typ[index], (*wval)[i]) + " ) OR ( " + sel_field[j][index] + " IS NULL )) ";
                            else
                                str =  " ( "  + sel_field[j][index] + " ) " + op + " " + con->getValue(sel_typ[index], (*wval)[i]);
                        }
                    }
                }
                else
                {
                    unsigned long j;
                    this->errorfound = 1;
                    index = std::string::npos;
                    msg.perror(E_SELID,
                            "Spaltenid <%s> für Whereclause in Sicht <%s> "
                                "nicht gefunden", (*wcol)[i].c_str(), (schema
                                    + ":" + name).c_str());
                    for (j = 0; j < sel_id.size(); ++j)
                        msg.line("%s", sel_id[j].c_str());
                    this->errorfound = 1;
                }

                if (  sel_musthaving[j][index] )
                    having += " " + hconcat + no + str;
                else
                    where += " " + wconcat + no + str;
            }
            if ( where != "" )
                laststm += stmconcat + "( " + where + " )";
        }

        if (params != NULL && params->size() > 0)
        {
            for (index = 0; 1; index++)
            {
                char str[16];
                sprintf(str, "#%d", (int) index);

                if ((i = laststm.find(str)) == std::string::npos) break;
                laststm.replace(i, strlen(str), (*params)[index]);
            }
        }

        if (sel_groupby[j].size() != 0)
        {
            laststm += " GROUP BY " + sel_groupby[j][0];
            for (i = 1; i < sel_groupby[j].size(); ++i)
                    laststm += ", " + sel_groupby[j][i] + "";
        }

        if ( having != "" )
            laststm += " HAVING " + having;
    }


    if (sort != NULL && sort->size() > 0)
    {
        laststm += " ORDER BY ";
        for (i = 0; i < sort->size();)
        {
            std::string str = (std::string)((*sort)[i]);
            std::string asc;
            if ( str[0] == '!' )
            {
            	str.replace(0, 1, "");
            	asc = "DESC";
            }
            else
            {
            	asc = "ASC";
            }
            if ((vi = std::find(sel_id.begin(), sel_id.end(), str)) != sel_id.end()) laststm += "\""
                    + *vi + "\" " + asc;
            else
            {
                this->errorfound = 1;
                msg.perror(E_SELID,
                        "Spaltenid <%s> zum Sortieren in Sicht <%s> "
                            "nicht gefunden",
                        ((std::string) (*sort)[i]).c_str(), (schema + ":"
                                + name).c_str());
            }
            ++i;
            if (i < sort->size()) laststm += ", ";

        }
    }

    if (this->errorfound) return "";
    else return laststm;

}

void
DbQuery::setName(std::string schema, std::string name, CsList *cols, std::string unionnum)
{

    DbJoin *cjoin = NULL;
    DbCursor *ccur, *tcur = NULL, *wcur = NULL;
    DbTable *tab = NULL;
    std::string stm;
    DbConnect::ResultVec r, *pr, empty;
    int deepid;
    std::string lang;
    std::vector<std::string> tabs;
    int old_unionnum, offset_unionnum, count_unionnum;
    std::map<int,int> unionnum2count;
    std::map<int,int>::iterator ui;

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

    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "querycolumns");
    cjoin = dbadmin->p_getJoin();
    tabs.push_back(cjoin->add_table(tab)); /* 0 */
    dbadmin->release(tab);

    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "queryname");
    tabs.push_back(cjoin->add_table(tab, "queryid", "queryid", "=",
            DbJoin::LEFT)); /* 1 */
    dbadmin->release(tab);

    cjoin->add_where(0, 0, tabs[1], "schema", "=", "", "'" + schema + "'", 0,
            "AND");
    cjoin->add_where(0, 0, tabs[1], "query", "=", "", "'" + name + "'", 0, "");

    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "querycolnames"); /* 2 */
    tabs.push_back(cjoin->add_table(tab, "schema,query,t0.colid",
            "schema,query,colid", "=,=,=", DbJoin::LEFT));
    dbadmin->release(tab);

    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "querytables");
    tabs.push_back(cjoin->add_table(tab, tabs[0] + ".queryid," /* 3 */
    + tabs[0] + ".tabnum", "queryid,tabnum", "=,=", DbJoin::LEFT));
    dbadmin->release(tab);

    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "tablecolnames");
    tabs.push_back(cjoin->add_table(tab, "tschema,ttab," + tabs[0] + ".field",
            "schema,tab,colname", "=,=,=", DbJoin::LEFT)); /* 4 */
    dbadmin->release(tab);

    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "tablecolnames");
    tabs.push_back(cjoin->add_table(tab, tabs[3] + ".tschema," + tabs[3]
            + ".ttab," + tabs[0] + ".field || '_" + lang + "'",
            "schema,tab,colname", "=,=,=", DbJoin::LEFT)); /* 5 */
    dbadmin->release(tab);

    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "tableregexp");
    tabs.push_back(cjoin->add_table(tab, tabs[4] + ".regexp", "tableregexpid",
            "=", DbJoin::LEFT)); /* 6 */
    dbadmin->release(tab);

    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "translate");
    tabs.push_back(cjoin->add_table(tab, "regexphelp", "id", "=", DbJoin::LEFT)); /* 7 */

    cjoin->back(5);
    tabs.push_back(cjoin->add_table(tab, "regexphelp", "id", "=", DbJoin::LEFT)); /* 8 */
    dbadmin->release(tab);

    stm = "SELECT " + tabs[0] + ".tabnum," + tabs[0] + ".field," + tabs[0]
            + ".colid," + tabs[2] + ".text_" + lang + "," + tabs[4]
            + ".colname, COALESCE(" + tabs[4] + ".text_" + lang + "," + tabs[5]
            + ".text_" + lang + ")," + tabs[0] + ".fieldtyp," + tabs[0]
            + ".lang, " + tabs[0] + ".format, " + tabs[1] + ".unionnum, "
            + tabs[4] + ".dpytype, " + tabs[0] + ".groupby, " + tabs[0]
            + ".cannull, " + "tcn.text_" + lang + ", tcn.dpytype,"
            + " COALESCE(" + tabs[6] + ".regexp," + tabs[4] + ".regexp,"
            + tabs[5] + ".regexp, rtcn.regexp, tcn.regexp),"
            + " COALESCE(NULLIF(" + tabs[7] + ".text_" + lang + ",''),"
            + "          NULLIF(" + tabs[8] + ".text_" + lang + ",''),"
            + "          NULLIF(" + "trtcn.text_" + lang + ",''),"
            + "          NULLIF(" + "ttcn.text_" + lang + ",''),"
            + "          NULLIF(" + tabs[6] + ".regexphelp,''),"
            + "          NULLIF(" + tabs[4] + ".regexphelp,''),"
            + "          NULLIF(" + tabs[5] + ".regexphelp,''),"
            + "          NULLIF(" + "rtcn.regexphelp,''),"
            + "          NULLIF(" + "tcn.regexphelp,'')), " + tabs[0] + ".musthaving FROM ( "
            + cjoin->getFrom() + " LEFT JOIN " + dbadmin->getApplschema()
            + ".tablecolnames tcn " + " ON tcn.schema = '' AND tcn.tab = '' "
            + " AND tcn.colname = " + tabs[0] + ".field  LEFT JOIN "
            + dbadmin->getApplschema()
            + ".translate ttcn ON tcn.regexphelp = ttcn.id " + " LEFT JOIN "
            + dbadmin->getApplschema()
            + ".tableregexp rtcn ON tcn.regexp = rtcn.tableregexpid "
            + " LEFT JOIN  " + dbadmin->getApplschema()
            + ".translate trtcn ON rtcn.regexphelp = trtcn.id)" + " WHERE "
            + cjoin->getWhere();
    if (unionnum != "" && unionnum != "0") stm += " AND " + tabs[1]
            + ".unionnum = " + unionnum;
    stm += " ORDER BY " + tabs[1] + ".unionnum, " + tabs[0] + ".colnum";

    dbadmin->release(cjoin);
    ccur->open((char*) stm.c_str());

    cjoin = dbadmin->p_getJoin();
    tabs.clear();
    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "querytables");
    tabs.push_back(cjoin->add_table(tab));
    dbadmin->release(tab);

    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "queryname");
    tabs.push_back(cjoin->add_table(tab, "queryid", "queryid", "=",
            DbJoin::LEFT));
    dbadmin->release(tab);

    cjoin->add_where(0, 0, tabs[1], "schema", "=", "", "'" + schema + "'", 0,
            "AND");
    cjoin->add_where(0, 0, tabs[1], "query", "=", "", "'" + name + "'", 0, "");

    stm = "SELECT " + tabs[0] + ".deep, " + tabs[0] + ".fcols,  " + tabs[0]
            + ".tschema,  " + tabs[0] + ".ttab,  " + tabs[0] + ".tcols,  "
            + tabs[0] + ".op,  " + tabs[0] + ".typ,  " + tabs[0] + ".tabnum,  "
            + tabs[1] + ".unionnum "
        "FROM " + cjoin->getFrom() + "WHERE " + cjoin->getWhere();
    if (unionnum != "" && unionnum != "0") stm += " AND  " + tabs[1]
            + ".unionnum = " + unionnum;
    stm += "ORDER BY  " + tabs[1] + ".unionnum,  " + tabs[0] + ".tabid";

    dbadmin->release(cjoin);
    tcur->open((char*) stm.c_str());

    cjoin = dbadmin->p_getJoin();
    tabs.clear();
    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "querywheres");
    tabs.push_back(cjoin->add_table(tab));
    dbadmin->release(tab);

    tab = dbadmin->p_getTable(dbadmin->getApplschema(), "queryname");
    tabs.push_back(cjoin->add_table(tab, "queryid", "queryid", "=",
            DbJoin::LEFT));
    dbadmin->release(tab);

    cjoin->add_where(0, 0, tabs[1], "schema", "=", "", "'" + schema + "'", 0,
            "AND");
    cjoin->add_where(0, 0, tabs[1], "query", "=", "", "'" + name + "'", 0, "");

    stm = "SELECT " + tabs[0] + ".notoperator, " + tabs[0] + ".leftbrace, "
            + tabs[0] + ".lefttab, " + tabs[0] + ".leftvalue, " + tabs[0]
            + ".operator, " + tabs[0] + ".righttab, " + tabs[0]
            + ".rightvalue, " + tabs[0] + ".rightbrace, " + tabs[0]
            + ".booloperator, " + tabs[1] + ".unionnum "
        "FROM " + cjoin->getFrom() + "WHERE " + cjoin->getWhere();
    if (unionnum != "" && unionnum != "0") stm += " AND " + tabs[1]
            + ".unionnum = " + unionnum;
    stm += "ORDER BY " + tabs[1] + ".unionnum, " + tabs[0] + ".wherecol";

    dbadmin->release(cjoin);
    wcur->open((char*) stm.c_str());

    if ((pr = tcur->p_next()) != NULL) r = *pr;
    else r = empty;

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
                "      schema = '" + schema + "' AND query = '" + name
                    + "' AND unionnum = " + str;

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
                msg.pwarning(W_TABLOST, "Tabelle %s.%s existiert nicht mehr",
                        (char*) r[2], (char*) r[3]);
                dbadmin->release(tab);
            }
            else
            {
                act_join->back(deepid);
                tabnums[offset_unionnum + ((long) r[7])] = act_join->add_table(
                        (std::string)r[2]+ "." + (std::string) r[3],
                        (char *) r[1], (char*) r[4], (char*) r[5],
                        (long) r[6]);
                tables[ToString::convert((long) count_unionnum) + "_"
                        + tabnums[offset_unionnum + ((long) r[7])]] = tab;
                deepid++;
            }
        }

        if ((pr = tcur->p_next()) != NULL) r = *pr;
        else r = empty;
        if (tcur->eof())
        {
            joins.push_back(act_join);
            unionnum2count[old_unionnum] = joins.size() - 1;
        }
    }

    std::vector<std::string> act_sel_field;
    std::vector<std::string> act_groupby;
    std::vector<long> act_musthaving;
    std::string act_sel_fields;
    int first, n;
    int unionnumid;
    long sel_poscount;

    if ((pr = ccur->p_next()) != NULL) r = *pr;
    else r = empty;
    old_unionnum = -1;
    count_unionnum = -1;
    first = 1;
    sel_poscount = 0;

    while (!ccur->eof())
    {
        std::string field;

        if ((long) r[9] != old_unionnum)
        {
            if (old_unionnum != -1)
            {
                if ( sel_musthaving.size() > 0 && sel_musthaving[0].size() != act_musthaving.size() )
                {
                    msg.perror(E_SELFIELDS, "Anzahl der Selektfelder ist nicht gleich bei unionnum <%d>", old_unionnum );
                    this->errorfound = 1;

                    tcur->close();
                    ccur->close();
                    wcur->close();

                    dbadmin->release(ccur);
                    dbadmin->release(tcur);
                    dbadmin->release(wcur);
                    return;
                }
                sel_field.push_back(act_sel_field);
                sel_fields.push_back(act_sel_fields);
                sel_groupby.push_back(act_groupby);
                sel_musthaving.push_back(act_musthaving);
                first = 0;
            }

            old_unionnum = (long) r[9];
            if ( ( ui = unionnum2count.find(old_unionnum)) != unionnum2count.end() )
            {
                count_unionnum = ui->second;
            }
            else
            {
                msg.perror(E_UNIONNUM, "Unionnum ist unbekannt <%d>", old_unionnum );
                this->errorfound = 1;

                tcur->close();
                ccur->close();
                wcur->close();

                dbadmin->release(ccur);
                dbadmin->release(tcur);
                dbadmin->release(wcur);

                return;
            }

            offset_unionnum = count_unionnum * 1000;
            act_sel_field.clear();
            act_sel_fields.clear();
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
                sel_format.push_back((std::string) r[8]);
                sel_null.push_back((long) r[12]);
                sel_regexp.push_back((std::string) r[15]);
                sel_regexphelp.push_back((std::string) r[16]);

                if (!r[10].isnull && (long) r[10] != -1) sel_typ.push_back(
                        (long) r[10]);
                else if (!r[14].isnull && (long) r[14] != -1) sel_typ.push_back(
                        (long) r[14]);
                else if (!r[6].isnull && (long) r[6] != -1) sel_typ.push_back(
                        (long) r[6]);
                else sel_typ.push_back(-1);

                if ((std::string) r[3] != "") sel_name.push_back((char *) r[3]);
                else if ((std::string) r[5] != "") sel_name.push_back(
                        (char *) r[5]);
                else if ((std::string) r[13] != "") sel_name.push_back(
                        (char *) r[13]);
                else if ((std::string) r[2] != "") sel_name.push_back(
                        (char *) r[2]);
                else sel_name.push_back((char *) r[1]);
            }

            act_musthaving.push_back((long) (r[17]));

            if ((long) (r[11]))
            {
                if (cols == NULL || cols->empty() || cols->find((std::string) r[2]) != std::string::npos)
                {
                    if (tabnums[unionnumid] != "")
                    {
                        std::string s = tabnums[unionnumid] + "." + (std::string)(r[1]);
                        if ((long) r[7] != 0) s = s + "_" + lang;
                        act_groupby.push_back(s);
                    }
                    else
                    {
                        act_groupby.push_back((std::string)(r[2]));
                    }
                }
            }

            if (sel_typ[n] == -1)
            {
                if (!r[10].isnull && (long) r[10] != -1) sel_typ[n]
                        = (long) r[10];
                else if (!r[14].isnull && (long) r[14] != -1) sel_typ[n]
                        = (long) r[14];
                else if (!r[6].isnull && (long) r[6] != -1) sel_typ.push_back(
                        (long) r[6]);
                else sel_typ[n] = -1;
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
                    if ((long) r[7] != 0) field = tabnums[unionnumid] + ".\""
                            + (char*) r[1] + "_" + lang + "\"";
                    else field = tabnums[unionnumid] + ".\"" + (char*) r[1]
                            + "\"";

                }
            }
            else if ((long) r[7] != 0)
            {
                std::string str = (std::string) r[1];
                if (str[0] == '\'' && str[str.size() - 1] == '\'')
                    field = db->p_getConnect()->getValue(DbConnect::CHAR, msg.get(str.substr(1, str.size() - 2)));
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
                while (i < field.size() && (i = field.find("#mne_lang#", i))
                        != std::string::npos)
                {
                    if ((j = field.find_first_of("#'\"", i + 11))
                            != std::string::npos)
                    {
                        std::string::size_type k = j;
                        if (field[j] != '#') j--;
                        field.replace(i, j - i + 1, msg.get(field.substr(
                                i + 10, k - i - 10)));
                    }
                    i++;
                }

                i = 0;
                while (i < field.size() && (i = field.find("#", i))
                        != std::string::npos)
                {
                    j = field.find_first_of(".#", i + 1);
                    if (j != std::string::npos && field[j] != '#') field.replace(
                            i, j - i, tabnums[offset_unionnum + atoi(
                                    field.substr(i + 1, j - i - 1).c_str())]);
                    i++;
                }
            }

            if (sel_id[n][0] != '-' && (cols == NULL || cols->empty()
                    || cols->find((std::string) r[2]) != std::string::npos))
            {
                if (act_sel_fields != "") act_sel_fields += ",\n";
                act_sel_fields += field + " AS " + sel_id[n];
                if ( first ) sel_pos[sel_id[n]] = sel_poscount++;

            }
            act_sel_field.push_back(field);

        }
        else
        {
            msg.perror(E_TABID, "Kann Table mit Id %d nicht finden", unionnumid);
            this->errorfound = 1;

            tcur->close();
            ccur->close();
            wcur->close();

            dbadmin->release(ccur);
            dbadmin->release(tcur);
            dbadmin->release(wcur);

            return;
        }
        n++;

        if ((pr = ccur->p_next()) != NULL) r = *pr;
        else r = empty;
        if (ccur->eof())
        {
            if ( sel_fields.size() > 0 && sel_field[0].size() != act_sel_field.size() )
            {
                msg.perror(E_SELFIELDS, "Anzahl der Selektfelder ist nicht gleich bei unionnum <%d>", old_unionnum );
                this->errorfound = 1;

                tcur->close();
                ccur->close();
                wcur->close();

                dbadmin->release(ccur);
                dbadmin->release(tcur);
                dbadmin->release(wcur);

                return;
            }
            sel_fields.push_back(act_sel_fields);
            sel_field.push_back(act_sel_field);
            sel_groupby.push_back(act_groupby);
            sel_musthaving.push_back(act_musthaving);
        }
    }

    if ((pr = wcur->p_next()) != NULL) r = *pr;
    else r = empty;
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
                if ( ( ui = unionnum2count.find(old_unionnum)) != unionnum2count.end() )
                {
                    count_unionnum = ui->second;
                }
                else
                {
                    msg.perror(E_UNIONNUM, "Unionnum ist unbekannt <%d>", old_unionnum );
                    this->errorfound = 1;

                    tcur->close();
                    ccur->close();
                    wcur->close();

                    dbadmin->release(ccur);
                    dbadmin->release(tcur);
                    dbadmin->release(wcur);

                    return;
                }
                offset_unionnum = count_unionnum * 1000;
                act_join = joins[count_unionnum];
            }
        }

        t1 = t2 = "";

        if ((std::string) r[2] != ""
                && (i = tabnums.find( offset_unionnum + atoi((char*) r[2]))) != tabnums.end())
            t1 = i->second;

        if ((std::string) r[5] != ""
                && (i = tabnums.find( offset_unionnum + atoi((char*) r[5]))) != tabnums.end())
            t2 = i->second;

        v1     = ((char*) r[3]);
        v2     = ((char*) r[6]);
        notop  = (long) r[0];
        lb     = (long) r[1];
        op     = (char*) r[4];
        rb     = (long) r[7];
        boolop = (char*) r[8];

        if (t1 != "" && t2 == "" )
        {
            DbTable *t = tables[ToString::convert((long) count_unionnum) + "_"
                    + t1];
            DbTable::Column c = t->getColumn(v1);
            c.value = v2;
            c.can_null = 0;
            v2 = t->getValue(&c);
        }

        if (t1 == "" && t2 != "")
        {
            DbTable *t = tables[ToString::convert((long) count_unionnum) + "_"
                    + t2];
            DbTable::Column c = t->getColumn(v2);
            c.value = v1;
            c.can_null = 0;
            v1 = t->getValue(&c);
        }

        if ( t1 == "" && t2 == "" )
        {
            std::string::size_type i,j;
            i = 0;
            while (i < v1.size() && (i = v1.find("#", i)) != std::string::npos)
            {
                j = v1.find_first_of(".#", i + 1);
                if (j != std::string::npos && v1[j] != '#') v1.replace(
                        i, j - i, tabnums[offset_unionnum + atoi(
                                v1.substr(i + 1, j - i - 1).c_str())]);
                i++;
            }

            i = 0;
            while (i < v2.size() && (i = v2.find("#", i)) != std::string::npos)
            {
                j = v2.find_first_of(".#", i + 1);
                if (j != std::string::npos && v2[j] != '#') v2.replace(
                        i, j - i, tabnums[offset_unionnum + atoi(
                                v2.substr(i + 1, j - i - 1).c_str())]);
                i++;
            }

        }

        if ((pr = wcur->p_next()) != NULL) r = *pr;
        else r = empty;
        if (wcur->eof() || (long) r[9] != old_unionnum ) boolop = "";

        if ( v1 == "" && t1 == "" && v2 == "" && t2 == "" )
            op = "";

        act_join->add_where( notop, lb, t1, v1, op, t2, v2, rb, boolop);
    }

    tcur->close();
    ccur->close();
    wcur->close();

    dbadmin->release(ccur);
    dbadmin->release(tcur);
    dbadmin->release(wcur);

}

void
DbQuery::open(CsList *wcol, CsList *wval, CsList *wop, CsList *sort, CsList *params)
{
    cur->open((char *) mk_statement(wcol, wval, wop, sort, params).c_str());
}

DbConnect::ResultMat *
DbQuery::select(CsList *wcol, CsList *wval, CsList *wop, CsList *sort, CsList *params)
{
    db->p_getConnect()->execute((char *) mk_statement(wcol, wval, wop, sort,
            params).c_str());
    return db->p_getConnect()->p_get_result();

}
void
DbQuery::open(DbTable::ValueMap *w, CsList *wop, CsList *sort, CsList *params)
{
    cur->open((char *) mk_statement(w, wop, sort, params).c_str());
}

DbConnect::ResultMat *
DbQuery::select(DbTable::ValueMap *w, CsList *wop, CsList *sort, CsList *params)
{
    db->p_getConnect()->execute((char *) mk_statement(w, wop, sort, params).c_str());
    return db->p_getConnect()->p_get_result();
}

void
DbQuery::start_cols()
{
    act_sel_name = sel_name.begin();
    act_sel_id = sel_id.begin();
    act_sel_typ = sel_typ.begin();
    act_sel_format = sel_format.begin();
    act_sel_regexp = sel_regexp.begin();
    act_sel_regexphelp = sel_regexphelp.begin();
}

int
DbQuery::getCols(std::string *id, std::string *name, long *typ,
        std::string *format, std::string *regexp, std::string *regexphelp)
{

    if (act_sel_name == sel_name.end() || act_sel_id == sel_id.end()
            || act_sel_format == sel_format.end() || act_sel_typ
            == sel_typ.end() || act_sel_regexp == sel_regexp.end()
            || act_sel_regexphelp == sel_regexphelp.end()) return 0;

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

    return 1;

}

