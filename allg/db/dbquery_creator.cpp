#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <utils/tostring.h>
#include "database.h"
#include "dbconnect.h"
#include "dbcursor.h"
#include "dbtable.h"

#include "dbquery_creator.h"

DbQueryCreator::DbQueryCreator(Database *db) :
	msg("DbQueryCreator")
{
	this->db = db;
}

DbQueryCreator::~DbQueryCreator()
{
}

void DbQueryCreator::setName(std::string schema, std::string name, std::string unionnum)
{
	DbJoin *cjoin;
	DbCursor *jcur, *ccur,  *wcur;
	DbTable *tab;
	std::string stm;
	DbConnect::ResultVec *r;
	std::set<int> tabnums;
	std::string order;
	std::string act_tab;

	clear_join();
	clear_select();
	clear_where();

	this->schema = schema;
	this->name = name;
	this->unionnum = atoi(unionnum.c_str());

	stm = "SELECT "
		"    queryid, unionall, selectdistinct "
		"FROM "
		"    " + db->getApplschema() + ".queryname "
		"WHERE "
		"      schema = '" + schema + "' AND query = '" + name + "' AND unionnum = " + unionnum;

	db->p_getConnect()->execute((char *) stm.c_str());
	if (db->p_getConnect()->have_result())
	{
		DbConnect::ResultVec *r = db->p_getConnect()->p_get_first_result();
		queryid = (char *) ((*r)[0]);
		unionall = (long) ((*r)[1]);
		distinct = (long) ((*r)[2]);
	}
	else
	{
		queryid = "";
		unionall = 1;
		distinct = 0;

		return;
	}

	cjoin = db->p_getJoin();
	jcur = db->p_getCursor();
	ccur = db->p_getCursor();
	wcur = db->p_getCursor();

	tab = db->p_getTable(db->getApplschema(), "querycolumns");
	act_tab = cjoin->add_table(tab);
	db->release(tab);

	order = act_tab + ".colnum";

	stm = "SELECT " + act_tab + ".tabnum, " + act_tab + ".field, " + act_tab
			+ ".colid, " + act_tab + ".fieldtyp, " + act_tab + ".lang, "
			+ act_tab + ".format, " + act_tab + ".groupby, " + act_tab
			+ ".cannull, " + act_tab + ".musthaving, ";

	tab = db->p_getTable(db->getApplschema(), "queryname");
	act_tab = cjoin->add_table(tab, "queryid", "queryid", "=",
			DbJoin::LEFT);
	db->release(tab);

	cjoin->add_where(0, 0, act_tab, "schema", "=", "", "'" + schema +  "'", 0, "AND");
	cjoin->add_where(0, 0, act_tab, "query", "=", "", "'"  + name + "'", 0, "AND");
	cjoin->add_where(0, 0, act_tab, "unionnum", "=", "", unionnum, 0, "");

	tab = db->p_getTable(db->getApplschema(), "querycolnames");
	act_tab = cjoin->add_table(tab, "schema,query,t0.colid", "schema,query,colid", "=,=",
			DbJoin::LEFT);
	db->release(tab);

	stm += act_tab + ".colid, " + act_tab + ".text_de, " + act_tab
			+ ".text_en " + " FROM " + cjoin->getFrom() + " WHERE "
			+ cjoin->getWhere() + " ORDER BY " + order;

	db->release(cjoin);

	ccur->open((char *) stm.c_str());

	stm = "SELECT "
		"    deep, tabnum, joindefid, fcols, tschema, ttab, tcols, op, typ "
		"FROM "
		"    " + db->getApplschema() + ".querytables "
		"WHERE "
		"      queryid = '" + queryid + "' "
		"ORDER BY"
		"    tabid";

	jcur->open((char*) stm.c_str());

	stm = "SELECT notoperator, leftbrace, lefttab, leftvalue, "
		"operator, "
		"righttab, rightvalue, rightbrace, booloperator "
		"FROM   " + db->getApplschema() + ".querywheres "
		"WHERE  queryid = '" + queryid + "' "
		"ORDER BY wherecol";

	wcur->open((char*) stm.c_str());


	r = jcur->p_next();
	while (!jcur->eof())
	{
		add_join( (long)(*r)[0],  (long)(*r)[1], (char*)(*r)[2],
				  (char*)(*r)[3], (char*)(*r)[4], (char*)(*r)[5],
				  (char*)(*r)[6], (char*)(*r)[7],  (long)(*r)[8]);
		r = jcur->p_next();
	}
	act_j_line = j_lines.end();

	r = ccur->p_next();
	while (!ccur->eof())
	{
		s_lines.push_back(SelectLine((long) (*r)[0], (char*) (*r)[1],
				(char*) (*r)[2], (long) (*r)[3], (long) (*r)[4],
				(char *) (*r)[5], (long) (*r)[6], (long) (*r)[7],
				(long) (*r)[8], (char*) (*r)[10],  (char*) (*r)[11]));
		r = ccur->p_next();
	}
    act_s_lines = s_lines.end();

	r = wcur->p_next();
	while (!wcur->eof())
	{
		w_lines.push_back(WhereLine((long) (*r)[0], (long) (*r)[1],
				(char*) (*r)[2], (char*) (*r)[3], (char*) (*r)[4],
				(char*) (*r)[5], (char*) (*r)[6], (long) (*r)[7],
				(char*) (*r)[8]));
		r = wcur->p_next();
	}
    act_w_lines = w_lines.end();

	jcur->close();
	ccur->close();
	wcur->close();

	db->release(ccur);
	db->release(jcur);
	db->release(wcur);
}

int DbQueryCreator::add_join( int deep, int tabnum, std::string joindefid,
		   std::string fcols,
		   std::string tschema, std::string ttab, std::string tcols,
		   std::string op, int typ)
{
	j_lines.push_back(JoinLine(db, deep, tabnum, joindefid, fcols, tschema, ttab, tcols, op, typ));
	return j_lines.size() - 1;
}

int DbQueryCreator::add_select(std::string tabnum, std::string field,
		std::string id, std::string lang, int typ, std::string format,
		int groupby, int musthaving, std::string text_de, std::string text_en)
{
	std::vector<JoinLine>::iterator i;
	DbTable::ColumnMap::iterator c;
	std::string f;
	int tab;
	long l;
	std::string::size_type n;

	tab = atoi(tabnum.c_str());
	l = (lang != "");

	if (tab < 0 || tabnum.find_first_of("0123456789") == std::string::npos)
	{
		if (typ == 0)
			typ = 2;
		s_lines.push_back(SelectLine(-1, field, id, typ, l, format, groupby,
				0, musthaving, text_de, text_en));
		return (s_lines.size() - 1);
	}

	for (i = j_lines.begin(); i != j_lines.end(); ++i)
	{
		if (i->tabnum == tab)
		{
			if (l)
			{
				if ((n = field.rfind("_")) != std::string::npos)
					field = field.substr(0, n);

				f = field + "_" + lang;
			}
			else
				f = field;

			if ( (c = i->tab->p_getColumns()->find(f)) == i->tab->p_getColumns()->end() )
				if (field[0] != '(')
					return -1;

			if (id == "")
			{
				id = field;
				transform(id.begin(), id.end(), id.begin(), tolower);
			}

			s_lines.push_back(SelectLine(tab, field, id, c->second.typ, l,
					format, groupby,  c->second.can_null
							& DbTable::Column::DBNULL, musthaving, text_de, text_en));
			return (s_lines.size() - 1);
		}
	}

	return -1;
}


int DbQueryCreator::add_where(std::string notop, std::string leftbrace,
		std::string lefttab, std::string leftval, std::string op,
		std::string righttab, std::string rightval, std::string rightbrace,
		std::string boolop)
{
	std::vector<JoinLine>::iterator i;
	int tab;

	if (lefttab != "" && lefttab != "f")
	{
		tab = atoi(lefttab.c_str());

		for (i = j_lines.begin(); i != j_lines.end(); ++i)
		{
			if (i->tabnum == tab)
				break;
		}

		if (i == j_lines.end())
			return -1;
	}

	if (righttab != "" && righttab != "f")
	{
		tab = atoi(righttab.c_str());

		for (i = j_lines.begin(); i != j_lines.end(); ++i)
		{
			if (i->tabnum == tab)
				break;
		}

		if (i == j_lines.end())
			return -1;
	}

	w_lines.push_back(WhereLine(notop != "", leftbrace != "", lefttab, leftval,
			op, righttab, rightval, rightbrace != "", boolop));

	return w_lines.size() - 1;

}

void DbQueryCreator::add_need_tab(int tabnum, std::set<int> &need_tab)
{
	std::vector<JoinLine>::iterator jl;
	int deep;
	int tabid;

	for (jl = j_lines.begin(); jl != j_lines.end(); ++jl)
	{
		if (jl->tabnum == tabnum)
		{
			tabid = jl - j_lines.begin();
			deep = jl->deep;

			if (need_tab.find(tabid) == need_tab.end())
			{
				need_tab.insert(tabid);
				--jl;

				while (jl != j_lines.begin() && jl->deep != 0)
				{
					tabid = jl - j_lines.begin();
					if (deep > jl->deep)
					{
						if (need_tab.find(tabid) == need_tab.end())
							need_tab.insert(tabid);
						deep = jl->deep;
					}
					--jl;
				}
			}
			return;
		}
	}

	msg.perror(E_NOTAB, "Tabelle mit Nummber %d wurde nicht gefunden", tabnum);

}

int DbQueryCreator::del(std::string queryid)
{
	DbTable *tab;
	DbTable *col;
	DbTable *whe;
	DbTable *cdn;
	DbTable::ValueMap where;
    std::string stm;

	int result = 0;

	cdn = db->p_getTable(db->getApplschema(), "queryname");
	tab = db->p_getTable(db->getApplschema(), "querytables");
	col = db->p_getTable(db->getApplschema(), "querycolumns");
	whe = db->p_getTable(db->getApplschema(), "querywheres");

	where["queryid"] = queryid;

	result += whe->del(&where);
	result += col->del(&where);
	result += tab->del(&where);
	result += cdn->del(&where);

	stm = "DELETE FROM " + db->getApplschema() + ".querycolnames "
	      "WHERE schema || '.' || query not in ( SELECT DISTINCT schema || '.' || query FROM " + db->getApplschema() + ".queryname )";

	db->p_getConnect()->execute((char *) stm.c_str());

    db->release(tab);
	db->release(col);
	db->release(whe);
	db->release(cdn);

	return result;
}

int DbQueryCreator::save(std::string schema, std::string name, int unionnum, std::string queryid, int unionall, int distinct, int copy)
{

	DbTable *nam;
	DbTable *tab;
	DbTable *col;
	DbTable *whe;
	DbTable *cdn;
	DbTable::ValueMap where;
	DbTable::ValueMap value;

	std::set<int> check_tab;
	std::set<int> need_tab;
	std::set<int>::iterator nt;

	std::vector<SelectLine>::iterator sl;
	std::vector<WhereLine>::iterator wl;
    std::string id;
    std::string stm;

	int result = 0;

	if ( copy )
	{
		queryid = "################";
		name = name + "Copy";
	}

	if ( queryid == "################" && this->schema == schema && this->name == name && this->unionnum == unionnum )
	{
		msg.perror(E_DUPQUERY, "Query %s:%s:%d existiert schon", this->schema.c_str(), this->name.c_str(), unionnum);
		return -1;
	}

	if ( queryid == "################")
		id = db->p_getConnect()->mk_index();
	else
		id = this->queryid;

	check_tab.insert(0);
	need_tab.insert(0);
	for (sl = s_lines.begin(); sl != s_lines.end(); ++sl)
	{
		if (sl->tabnum >= 0)
		{
			if (check_tab.find(sl->tabnum) == check_tab.end())
			{
				check_tab.insert(sl->tabnum);
				add_need_tab(sl->tabnum, need_tab);
			}
		}
		else if (sl->tabnum == -1)
		{
			std::string::size_type i, j, tabnum;
			std::string field = sl->field;

			while (i < field.size() && (i = field.find("#mne_langid#", i))
					!= std::string::npos)
				field.erase(i, 12);

			i = 0;
			while (i < field.size() && (i = field.find("#mne_lang#", i))
					!= std::string::npos)
			{
				field.erase(i, 10);
				i++;
			}

			i = 0;
			while (i < field.size() && (i = field.find("#", i))
					!= std::string::npos)
			{
				if (isdigit(field[i + 1]))
				{
					if ((j = field.find(".", i + 1)) != std::string::npos)
					{
						tabnum = atoi(field.substr(i + 1, j - i - 1).c_str());
						if (check_tab.find(tabnum) == check_tab.end())
						{
							check_tab.insert(tabnum);
							add_need_tab(tabnum, need_tab);
						}
					}
				}
				i++;
			}
		}

	}

	for (wl = w_lines.begin(); wl != w_lines.end(); ++wl)
	{
		if (wl->lefttab != "" && check_tab.find(atoi(wl->lefttab.c_str()))
				== check_tab.end())
		{
			check_tab.insert(atoi(wl->lefttab.c_str()));
			add_need_tab(atoi(wl->lefttab.c_str()), need_tab);
		}

		if (wl->righttab != "" && check_tab.find(atoi(wl->righttab.c_str()))
				== check_tab.end())
		{
			check_tab.insert(atoi(wl->righttab.c_str()));
			add_need_tab(atoi(wl->righttab.c_str()), need_tab);
		}
	}

	nam = db->p_getTable(db->getApplschema(), "queryname");
	tab = db->p_getTable(db->getApplschema(), "querytables");
	col = db->p_getTable(db->getApplschema(), "querycolumns");
	whe = db->p_getTable(db->getApplschema(), "querywheres");
	cdn = db->p_getTable(db->getApplschema(), "querycolnames");

	stm = "DELETE FROM " + db->getApplschema() + ".querycolnames "
	      "WHERE schema || '.' || query not in ( SELECT DISTINCT schema || '.' || query FROM " + db->getApplschema() + ".queryname )";

	db->p_getConnect()->execute((char *) stm.c_str());

	where["schema"] = schema;
	where["query"] = name;
	result += cdn->del(&where);

	where.clear();
	where["queryid"] = id;
	result += col->del(&where);
	result += whe->del(&where);
	result += tab->del(&where);
	result += nam->del(&where);


	if (!s_lines.empty())
	{
		value["queryid"] = id;
		value["schema"] = schema;
		value["query"] = name;
		value["selectdistinct"] = distinct;
		value["unionall"] = unionall;
		value["unionnum"] = unionnum;
		result += nam->insert(&value);

		value.clear();
		value["queryid"] = id;

		for (nt = need_tab.begin(); nt != need_tab.end(); ++nt)
		{
			value["joindefid"] = j_lines[*nt].joindefid;
			value["fcols"] = j_lines[*nt].fcols;
			value["tschema"] = j_lines[*nt].tschema;
			value["ttab"] = j_lines[*nt].ttab;
			value["tcols"] = j_lines[*nt].tcols;
			value["op"] = j_lines[*nt].op.c_str();
			value["deep"] = j_lines[*nt].deep;
			value["tabnum"] = j_lines[*nt].tabnum;
			value["typ"] = j_lines[*nt].typ;
			value["tabid"] = *nt;
			result += tab->insert(&value);
		}

		value.clear();
		value["queryid"] = id;

		for (sl = s_lines.begin(); sl != s_lines.end(); ++sl)
		{
			value["colnum"] = sl - s_lines.begin();
			value["tabnum"] = sl->tabnum;
			value["field"] = sl->field.c_str();
			value["fieldtyp"] = sl->typ;
			value["colid"] = sl->id;
			value["lang"] = sl->lang;
			value["format"] = sl->format;
			value["groupby"] = sl->groupby;
            value["cannull"] = sl->cannull;
            value["musthaving"] = sl->musthaving;
			result += col->insert(&value);
		}

		value.clear();
		value["schema"] = schema;
		value["query"]   = name;

		for (sl = s_lines.begin(); sl != s_lines.end(); ++sl)
		{
			value["colid"] = sl->id;
			value["text_de"] = sl->text_de;
			value["text_en"] = sl->text_en;
			result += cdn->insert(&value);
		}

		value.clear();
		value["queryid"] = id;

		for (wl = w_lines.begin(); wl != w_lines.end(); ++wl)
		{
			value["wherecol"] = wl - w_lines.begin();
			wl->notop ? value["notoperator"] = 1 : value["notoperator"] = 0;
			wl->leftbrace ? value["leftbrace"] = 1 : value["leftbrace"] = 0;
			value["lefttab"] = wl->lefttab;
			value["leftvalue"] = wl->leftval;
			value["operator"] = wl->op;
			value["righttab"] = wl->righttab;
			value["rightvalue"] = wl->rightval;
			wl->rightbrace ? value["rightbrace"] = 1 : value["rightbrace"] = 0;
			value["booloperator"] = wl->boolop;

			result += whe->insert(&value);
		}

	}

	db->release(tab);
	db->release(col);
	db->release(whe);
	db->release(cdn);
	db->release(nam);

	if ( result == 0 )
		this->queryid = id;
	return result;
}
