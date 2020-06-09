#ifndef dbquery_creator_mne
#define dbquery_creator_mne

#include <string>
#include <vector>
#include <set>

#include <message/message.h>
#include "database.h"
#include "dbjoin.h"

class DbTable;

class DbQueryCreator
{
	Message msg;
	Database *db;

	class JoinLine
	{
	public:
		int deep;
		int tabnum;

		std::string joindefid;
		std::string fcols;
		std::string tschema;
		std::string ttab;
		std::string tcols;
		std::string op;
		int typ;

		Database *db;
		DbTable *tab;

		JoinLine()
		{
			deep = tabnum = typ = -1;
			tab = NULL;
			 db = NULL;
		};

		JoinLine(Database *db, int deep, int tabnum, std::string joindefid,
				std::string fcols, std::string tschema, std::string ttab,
				std::string tcols, std::string op, int typ)
		{
			this->deep = deep;
			this->tabnum = tabnum;
			this->joindefid = joindefid;
			this->fcols = fcols;
			this->tschema = tschema;
			this->ttab = ttab;
			this->tcols = tcols;
			this->op = op;
			this->typ = typ;

			this->db = db;
			this->tab = db->p_getTable(tschema, ttab);
		}

		JoinLine(const JoinLine &in)
		{
			this->deep = in.deep;
			this->tabnum = in.tabnum;
			this->joindefid = in.joindefid;
			this->fcols = in.fcols;
			this->tschema = in.tschema;
			this->ttab = in.ttab;
			this->tcols = in.tcols;
			this->op = in.op;
			this->typ = in.typ;

			this->db = in.db;
			this->tab = db->p_getTable(in.tab->getSchema(), in.tab->getName());

		}
		JoinLine &operator=(const JoinLine &in)
		{
			this->deep = in.deep;
			this->tabnum = in.tabnum;
			this->joindefid = in.joindefid;
			this->fcols = in.fcols;
			this->tschema = in.tschema;
			this->ttab = in.ttab;
			this->tcols = in.tcols;
			this->op = in.op;
			this->typ = in.typ;

			this->db = in.db;
			this->tab = db->p_getTable(in.tab->getSchema(), in.tab->getName());

			return *this;

		}
		~JoinLine()
		{
			if (this->tab != NULL)
			{
				db->release(this->tab);
				tab = NULL;
			}
		}
	};

	class SelectLine
	{
	public:
		int tabnum;
		std::string jname;
		std::string field;
		std::string id;
		std::string format;
		long typ;
		long lang;
		long groupby;
		long cannull;
		long musthaving;
		std::string text_de;
		std::string text_en;

		SelectLine()
		{
		    tabnum = typ = lang = groupby = cannull = musthaving = -1;
		}
		;
		SelectLine(int tabnum, std::string field, std::string id, long typ,
				long lang, std::string format, long groupby, long cannull, long musthaving,
				std::string text_de, std::string text_en)
		{
			this->tabnum = tabnum;
			this->field = field;
			this->id = id;
			this->lang = lang;
			this->typ = typ;
			this->format = format;
			this->groupby = groupby;
            this->cannull = cannull;
            this->musthaving = musthaving;
			this->text_de = text_de;
			this->text_en = text_en;
		}
	};

	class WhereLine
	{
	public:
		int notop;
		int leftbrace;
		std::string lefttab;
		std::string leftval;
		std::string op;
		std::string righttab;
		std::string rightval;
		int rightbrace;
		std::string boolop;

		WhereLine()
		{
		    notop = leftbrace = rightbrace = -1;
		}
		;
		WhereLine(int notop, int leftbrace, std::string lefttab,
				std::string leftval, std::string op, std::string righttab,
				std::string rightval, int rightbrace, std::string boolop)
		{
			this->notop = notop;
			this->leftbrace = leftbrace;
			this->lefttab = lefttab;
			this->leftval = leftval;
			this->op = op;
			this->righttab = righttab;
			this->rightval = rightval;
			this->rightbrace = rightbrace;
			this->boolop = boolop;
		}
	};

	std::string queryid;
	std::string schema;
	std::string name;
	int distinct;
	int unionall;
	int unionnum;

	std::vector<JoinLine> j_lines;
	std::vector<JoinLine>::iterator act_j_line;

	std::vector<SelectLine> s_lines;
	std::vector<SelectLine>::iterator act_s_lines;

	std::vector<WhereLine> w_lines;
	std::vector<WhereLine>::iterator act_w_lines;

	void add_need_tab(int tabnum, std::set<int> &need_tab);
public:

	enum ERROR_TYPE
	{
		E_NOTAB,
		E_DUPQUERY,
		E_MAXID
	};

	enum WARNING_TYPE
	{
		W_MAXID
	};

	DbQueryCreator(Database *db);
	~DbQueryCreator();

	void setName(std::string schema, std::string name, std::string unionnum);
    std::string getQueryid() { return queryid; }

	void clear_join()
	{
		j_lines.clear();
	}
	int add_join(int deep, int tabnum, std::string joindefid,
			std::string fcols, std::string tschema, std::string ttab,
			std::string tcols, std::string op, int typ);

	void clear_select()
	{
		s_lines.clear();
	}
	int add_select(std::string tabnum, std::string field, std::string id,
			std::string lang, int typ, std::string format, int groupby, int musthaving,
			std::string text_de, std::string text_en);

	void clear_where()
	{
		w_lines.clear();
	}
	int add_where(std::string notop, std::string leftbrace,
			std::string lefttab, std::string leftval, std::string op,
			std::string righttab, std::string rightval, std::string rightbrace,
			std::string boolop);

	int save(std::string schema, std::string name, int unionnum, std::string queryid, int unionall, int distinct, int copy = 0);
	int del(std::string id);

};

#endif /* dbquery_creator_mne */
