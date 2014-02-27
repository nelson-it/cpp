#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#if defined(__MINGW32__) || defined(__CYGWIN__) || defined(Darwin)
typedef signed int int32;
#define VARHDRSZ ((int32) sizeof(int32))
#else
#include <postgres.h>
#endif

#include <libpq/libpq-fs.h>

#include "pgtable.h"
#include "pgtypes.h"

#define TAB_SEL "SELECT "\
	"a.attrelid, "   \
	"a.attnum, "   \
	"a.attname, "   \
	"a.atttypid, "\
	"a.atttypmod, "\
	"a.attnotnull, "\
	"a.atthasdef, "\
	"a.adsrc, "\
	"NULL, " \
	"c.relkind, " \
	"a.attname," \
	"a.attname," \
	"''," \
	"''," \
	"''," \
	"-1" \
    "from "\
	"( pg_catalog.pg_attribute a1 LEFT JOIN pg_catalog.pg_attrdef a2 " \
	"  ON a1.attrelid = a2.adrelid and a1.attnum = a2.adnum ) a, " \
	"pg_catalog.pg_class c, "\
	"pg_catalog.pg_namespace n "\
    "where "\
    "    a.attrelid = c.oid "\
    "and a.attnum > 0::pg_catalog.int2 "\
    "and a.attisdropped  = false "\
    "and ( c.relkind  = 'r' OR c.relkind = 'v' )"\
    "and c.relnamespace  = n.oid "\
    "and n.nspname  = '" + this->schema + "' "\
    "and c.relname = '" + this->name + "' ";\

#define TAB_SEL_EX "SELECT "\
	"a1.attrelid,"\
	"a1.attnum,"\
	"a1.attname,"\
	"a1.atttypid,"\
	"a1.atttypmod,"\
	"a1.attnotnull,"\
	"a1.atthasdef,"\
	"a2.adsrc,"\
	"a3.blobnum,"\
    "c.relkind,"\
    "COALESCE( NULLIF(a4.text_de,''), NULLIF(a6.text_de,''), a1.attname),"\
    "COALESCE( NULLIF(a4.text_en,''), NULLIF(a6.text_en,''), a1.attname),"\
    "COALESCE( NULLIF(a5.regexp,''),  NULLIF(a4.regexp,''), NULLIF(a7.regexp,''), NULLIF(a6.regexp,'') ),"\
	"COALESCE( NULLIF(ta5.text_de,''), NULLIF(ta4.text_de,''), NULLIF(ta7.text_de,''), NULLIF(ta6.text_de,''), NULLIF(a5.regexphelp,''), NULLIF(a4.regexphelp,''), NULLIF(a7.regexphelp,''), NULLIF(a6.regexphelp,''), '' ) as regexphelp_de," \
	"COALESCE( NULLIF(ta5.text_en,''), NULLIF(ta4.text_en,''), NULLIF(ta7.text_en,''), NULLIF(ta6.text_en,''), NULLIF(a5.regexphelp,''), NULLIF(a4.regexphelp,''), NULLIF(a7.regexphelp,''), NULLIF(a6.regexphelp,''), '' ) as regexphelp_de," \
    "COALESCE( a4.dpytype, a6.dpytype, -1 ) "\
  "from "\
  "((((((( "\
   "( pg_catalog.pg_attribute a1 LEFT JOIN pg_catalog.pg_attrdef a2  ON a1.attrelid = a2.adrelid and a1.attnum = a2.adnum )  "\
   " LEFT JOIN mne_catalog.blobcols a3 ON a1.attrelid = a3.blobrelid and a1.attnum = a3.blobnum )   "\
   " LEFT JOIN  mne_application.tablecolnames a4  ON  a4.schema = '" + this->schema + "' AND  a4.tab = '" + this->name + "' AND a4.colname = a1.attname "\
   "       LEFT JOIN mne_application.translate ta4 ON a4.regexphelp = ta4.id ) "\
   "       LEFT JOIN mne_application.tableregexp a5 ON a4.regexp = a5.tableregexpid  "\
   "       LEFT JOIN mne_application.translate ta5 ON a5.tableregexpid = ta5.id )) "\
   "   LEFT JOIN mne_application.tablecolnames a6 ON  a6.schema = '' AND a6.tab = '' AND a6.colname = a1.attname "\
   "       LEFT JOIN mne_application.translate ta6 on a6.regexphelp = ta6.id )   "\
   "       LEFT JOIN mne_application.tableregexp a7  ON a6.regexp = a7.tableregexpid  "\
   "       LEFT JOIN mne_application.translate ta7  ON a7.regexphelp = ta7.id ) ) ,   "\
"pg_catalog.pg_class c, "\
"pg_catalog.pg_namespace n "\
"where "\
"    a1.attrelid = c.oid "\
"and a1.attnum > 0::pg_catalog.int2 "\
"and a1.attisdropped  = false "\
"and ( c.relkind  = 'r' OR c.relkind = 'v' )"\
"and c.relnamespace  = n.oid "\
"and n.nspname  = '" + this->schema + "' "\
"and c.relname = '" + this->name + "'" ;

#define TAB_ALL \
    "SELECT " \
        "n.nspname, "\
	"c.relname, "\
	"c.relacl,   "\
	"c.relkind  "\
    "FROM "\
        "( pg_catalog.pg_namespace n LEFT JOIN pg_catalog.pg_class c " \
	"  ON ( n.oid = c.relnamespace ))" \
    "WHERE " \
        "( c.relkind = 'r' OR c.relkind is null OR c.relkind = 'v' ) " \
        " AND NOT n.nspname LIKE 'pg_temp%' "\
    "ORDER BY " \
        "nspname, relname"

#define TAB_PRIMARY \
	"SELECT  DISTINCT t1.column_name AS column "\
    "FROM ((information_schema.table_constraints t0 "\
    "       LEFT JOIN information_schema.key_column_usage t1 "\
    "            ON ( t0.constraint_catalog = t1.constraint_catalog AND t0.constraint_name = t1.constraint_name AND t0.constraint_schema = t1.constraint_schema) )) "\
    "WHERE (  t0.constraint_type = E'PRIMARY KEY'   ) "\
    "  AND (   t0.table_schema = E'" + getSchema() + "' AND t0.table_name = E'" + getDbname() + "')"

PgTable::PgTable(std::string schema, std::string name) :
	msg("PgTable")
{
	setName(schema, name);
	dbconnect = this;
}

PgTable::PgTable(std::string name) :
	msg("PgTable")
{
	setName(name);
	dbconnect = this;
}

PgTable::PgTable() :
	msg("PgTable")
{
	relid = -1;
	dbconnect = this;
}

PgTable::PgTable(PgConnect *con, std::string schema, std::string name) :
	PgConnect(con), msg("PgTable")
{
	setName(schema, name);
	dbconnect = this;
}

PgTable::PgTable(PgConnect *con, std::string name) :
	PgConnect(con), msg("PgTable")
{
	setName(name);
	dbconnect = this;
}

PgTable::PgTable(PgConnect *con) :
	PgConnect(con), msg("PgTable")
{
	relid = -1;
	dbconnect = this;
}

PgTable::~PgTable()
{
}

std::string PgTable::getTypestring(const Column *c)
{
	switch (c->typ)
	{
	case BOOL:
		return "BOOLEAN";
		break;
	case CHAR:
	{
		char str[32];
		if (c->size == 0)
			return "VARCHAR";
		else
			sprintf(str, "VARCHAR(%d)", c->size);
		return str;
	}
		break;
	case SHORT:
		return "SMALLINT";
		break;
	case LONG:
		return "INTEGER";
		break;
	case FLOAT:
		return "REAL";
		break;
	case DOUBLE:
		return "DOUBLE PRECISION";
		break;
	default:
		msg.perror(UNKNOWN_TYPE, "Der Typ %d wird nicht unterstützt", c->typ);
		return "";
		break;
	}
	return "";
}

std::string PgTable::getDefaultstring(const Column *col)
{
	std::string result;

	if (col->value != "" || col->can_null == Column::NOTNULL_DEF
			|| col->can_null == Column::NULL_DEF)
		result = " DEFAULT " + DbTable::getValue(col) + " ";

	return result;
}

std::string PgTable::getColumnstring(std::string name, const Column *col)
{
	std::string result;

	result = name + " " + getTypestring(col) + getDefaultstring(col);

	if (col->can_null == Column::NOTNULL_DEF || col->can_null
			== Column::NOTNULL_NOTDEF)
		result += " NOT NULL ";

	return result;
}

void PgTable::setName(std::string name, int ready)
{

	std::string::size_type i;

	if ((i = name.find_first_of('.')) != std::string::npos)
	{
		setName(name.substr(0, i), name.substr(i + 1), ready);
	}
	else
	{
		setName(getCurschema(), name, ready);
	}
}

void PgTable::setName(std::string schema, std::string name, int ready)
{
	std::string stm;
	ResultMat::iterator r;
	ColumnMapAll::iterator a;
	Column c;
	int error_found = 0;

	if ((this->name != "" && this->name != name) || (this->schema != ""
			&& this->schema != schema))
	{
		msg.perror(TABNAME_CHANGE, "Tabellenname kann nicht "
			"von %s.%s zu %s.%s verändert werden", this->schema.c_str(),
				this->name.c_str(), schema.c_str(), name.c_str());
		return;
	}

	this->schema = schema;
	this->name = name;

	if ((a = all_cols.find(schema + "." + name)) != all_cols.end())
	{
		cols = a->second;
		typ = all_typ[schema + "." + name];
	}
	else
	{
		ignore_oid = 1;

		if (have_extension())
			stm = TAB_SEL_EX
		else
			stm = TAB_SEL

		relid = -1;
		cols.clear();

		if (schema == "pg_catalog")
		{
			c.num = -1;
			c.int_typ = OIDOID;
			c.typ = DbConnect::LONG;
			c.size = 0;
			c.can_null = 0;
			c.value = "";
			c.name = "oid";
			cols[c.name] = c;
		}

		error_found = execute((char *) stm.c_str());

		if (!result.empty())
		{
			this->typ = *((char*) result[0][9]);
		}

		for (r = result.begin(); r != result.end(); ++r)
		{

			relid = *(long*) (*r)[0].value;
			c.num = *(long*) (*r)[1].value;
			c.int_typ = *(long*) (*r)[3].value;
			c.typ = int2ext_pgtype(c.int_typ);
			c.size = *(long*) (*r)[4].value - VARHDRSZ;

			c.can_null = 0;
			c.value = "";

			if (!*(long*) (*r)[5].value)
				c.can_null = Column::DBNULL;
			if (*(long*) (*r)[6].value)
			{
				c.can_null += Column::DBDEF;
				c.value = (char *) (*r)[7].value;
			}

			if (c.value[0] == '\'')
				c.value = c.value.substr(1, c.value.find("::") - 2);

			if (c.size < 0)
				c.size = 0;

			c.name = (char *) (*r)[2].value;
			c.text["de"] = (char *) (*r)[10].value;
			c.text["en"] = (char *) (*r)[11].value;
			c.regexp  = (char *) (*r)[12].value;
			c.regexphelp["de"]  = (char *) (*r)[13].value;
			c.regexphelp["en"]  = (char *) (*r)[14].value;
			c.dpytyp  = *(long *)(*r)[15].value;
			cols[c.name] = c;
		}
	}

	if (cols.find("createuser") != cols.end() && cols.find("createdate")
			!= cols.end() && cols.find("modifyuser") != cols.end()
			&& cols.find("modifydate") != cols.end())
		have_usertime_columns = 1;
	else
		have_usertime_columns = 0;

	if (ready)
		end();

	ignore_oid = 0;
	if (error_found == 0)
	{
		all_cols[this->schema + "." + this->name] = cols;
		all_typ[this->schema + "." + this->name] = this->typ;
	}

}

int PgTable::create(std::string schema, std::string name, ColumnMap *c,
		int ready)
{
	setName(schema, name);
	return create(c, ready);
}

int PgTable::create(std::string name, ColumnMap *c, int ready)
{
	setName(name);
	return create(c, ready);
}

int PgTable::create(ColumnMap *c, int ready)
{

	std::string stm;
	ColumnMap::iterator i;
	int first;
	int result;

	if (this->name == "")
	{
		msg.perror(NO_TABLENAME, "Kein Tabellenname beim Erzeugen der Tabelle "
			"vorhanden");
		if (ready)
			end();
		return -NO_TABLENAME;
	}

	setName(schema, name);

	if (!cols.empty())
	{
		msg.perror(TABLE_EXISTS, "kann Tabelle %s nicht erzeugen - "
			"Tabelle existiert", name.c_str());
		if (ready)
			end();
		return -TABLE_EXISTS;
	}

	stm = "CREATE TABLE " + getDbfullname() + "( ";

	for (first = 1, i = c->begin(); i != c->end(); ++i, first = 0)
	{
		if (!first)
			stm += " , ";

		stm += getColumnstring(i->first, &(i->second));
	}

	stm += " );";

	result = execute(stm.c_str(), ready);
	del_allcolumns();
	setName(schema, name, ready);

	return result;
}

int PgTable::rename(std::string newschema, std::string newname, int ready)
{
	int result = 0;
	std::string cmd;

	if ( newschema != this->schema )
	{
	    cmd = "ALTER TABLE " + getDbfullname() + " SET SCHEMA \"" + newschema + "\"";
		result = execute(cmd.c_str());
	}

	if ( result == 0 && newname != this->name )
    {
		cmd = "ALTER TABLE " + getDbfullname() + " RENAME TO \"" + newname + "\"";
		result = execute(cmd.c_str());
    }

	if ( result == 0 )
	{
		this->name = newname;
		this->schema = newschema;
	}

	if (ready)
		end();

	return result;
}

int PgTable::remove(std::string name, int ready)
{

	int result;
	char stm[256];

	setName(name);

	sprintf(stm, "DROP TABLE %s", name.c_str());
	result = execute(stm, ready);

	del_allcolumns();
	setName(name, ready);

	if (!cols.empty())
	{
		msg.perror(TABLE_NOTDROPED, "kann Tabelle %s nicht löschen",
				name.c_str());
		del_allcolumns();
		return -TABLE_EXISTS;
	}

	return result;
}

int PgTable::chk_column(std::string name, Column col, int modify, int ready)
{
	ColumnMap::iterator c;
	int result;

	result = Column::D_OK;

	if ((c = cols.find(name)) != cols.end())
	{
		msg.pdebug(D_STM, "Spalte %s beim modifizieren vorhanden\n",
				name.c_str());
		if (c->second.typ != col.typ || c->second.size != col.size)
		{
			if (modify)
			{
				Column ncol = col;
				std::string stm;

				if (ncol.can_null == Column::NOTNULL_NOTDEF)
					ncol.can_null = Column::NOTNULL_DEF;

				mv_column(name, name + "_o", 0);
				add_column(name, ncol, 0);

				if ((ncol.typ == DbConnect::SHORT || ncol.typ
						== DbConnect::LONG) && c->second.typ == CHAR)
					stm = "update " + getDbfullname() + " set " + name
							+ " = to_number(" + name + "_o, '99999999999999')";

				else if ((ncol.typ == DbConnect::FLOAT || ncol.typ
						== DbConnect::DOUBLE) && c->second.typ == CHAR)
					stm = "update " + getDbfullname() + " set " + name
							+ " = to_number(" + name
							+ "_o, '999999999999999.999999999999999')";
				else if ((c->second.typ == DbConnect::BOOL ) &&
						  (    ncol.typ == DbConnect::SHORT
						    || ncol.typ == DbConnect::LONG
						    || ncol.typ == DbConnect::FLOAT
						    || ncol.typ == DbConnect::DOUBLE))
					stm = "update " + getDbfullname() + " set " + name + " = "
							+ "CASE WHEN " + name + "_o = true THEN 1 ELSE 0 END";
				else if ((ncol.typ == DbConnect::BOOL ) &&
						  (    c->second.typ == DbConnect::SHORT
						    || c->second.typ == DbConnect::LONG
						    || c->second.typ == DbConnect::FLOAT
						    || c->second.typ == DbConnect::DOUBLE))
					stm = "update " + getDbfullname() + " set " + name + " = "
							 + name + "_o != 0";
				else
					stm = "update " + getDbfullname() + " set " + name + " = "
							+ name + "_o";

				execute(stm.c_str());

				if (col.can_null == Column::NOTNULL_NOTDEF)
				{
					stm = "ALTER TABLE " + getDbfullname() + " ALTER " + name
							+ " " + "DROP DEFAULT";
					execute(stm.c_str());
				}

				del_column(name + "_o");

				if (get_error())
					result |= Column::D_TYP;
			}
			else
			{
				if (c->second.typ != col.typ)
					result |= Column::D_TYP;
				if (c->second.size != col.size)
					result |= Column::D_SIZE;
			}
		}

		if (result == Column::D_OK && (c->second.can_null != col.can_null
				|| c->second.value != col.value))
		{

			if (modify)
			{
				std::string stm;
				stm = "ALTER TABLE " + getDbfullname() + " ALTER " + name;

				if (col.can_null == Column::NOTNULL_DEF || col.can_null
						== Column::NULL_DEF)
				{
					stm += " SET " + getDefaultstring(&col);
					if (execute(stm.c_str()))
						result |= Column::D_VALUE;

				}
				else
				{
					stm += " DROP DEFAULT";

					if (execute(stm.c_str()))
						result |= Column::D_VALUE;
				}

				if (((col.can_null == Column::NULL_DEF || col.can_null
						== Column::NULL_NOTDEF) && !c->second.can_null)
						|| ((col.can_null == Column::NOTNULL_DEF
								|| col.can_null == Column::NOTNULL_NOTDEF)
								&& c->second.can_null))
				{

					if (col.can_null == Column::NULL_DEF || col.can_null
							== Column::NULL_NOTDEF)
					{
						stm = "ALTER TABLE " + getDbfullname() + " ALTER "
								+ name;
						stm += " DROP NOT NULL";

						if (execute(stm.c_str()))
							result |= Column::D_VALUE;

					}
					else
					{
						stm = "UPDATE " + getDbfullname() + " SET " + name
								+ " = ";
						stm += DbTable::getValue(&col);
						stm += " WHERE " + name + " IS NULL";

						if (execute(stm.c_str()))
							result |= Column::D_VALUE;

						stm = "ALTER TABLE " + getDbfullname() + " ALTER "
								+ name;
						stm += " SET NOT NULL";

						if (execute(stm.c_str()))
							result |= Column::D_NULL;
					}
				}
			}
			else
			{
				if (c->second.can_null != col.can_null)
					result |= Column::D_NULL;
				else
					result |= Column::D_VALUE;
			}
		}

	}
	else
	{
		msg.pdebug(D_STM, "Spalte %s beim modifizieren nicht vorhanden\n",
				name.c_str());
		if (modify)
		{
			Column ncol = col;

			if (ncol.can_null == Column::NOTNULL_NOTDEF)
				ncol.can_null = Column::NOTNULL_DEF;

			add_column(name, ncol);

			if (col.can_null == Column::NOTNULL_NOTDEF)
			{
				std::string stm;

				stm = "ALTER TABLE " + getDbfullname() + " ALTER " + name + " "
						+ "DROP DEFAULT";

				execute(stm.c_str());

			}
		}
	}

	if (ready)
		end();

	del_allcolumns();
	return result;

}

int PgTable::add_column(std::string name, Column col, int ready)
{
	int result;
	std::string stm;

	if (this->name == "")
	{
		msg.perror(NO_TABLENAME,
				"Kein Tabellenname beim addieren einer Spalte "
					"vorhanden");
		if (ready)
			end();
		return -NO_TABLENAME;
	}

	stm = "ALTER TABLE " + getDbfullname() + " ADD " + name + " ";
	stm += getTypestring(&col);

	result = execute(stm.c_str());
	if (result)
	{
		del_allcolumns();
		setName(this->schema, this->name, ready);
		return result;
	}

	if (col.can_null == Column::NOTNULL_DEF || col.can_null == Column::NULL_DEF)
	{
		stm = "ALTER TABLE " + getDbfullname() + " ALTER " + name + " SET ";
		stm += getDefaultstring(&col);

		result = execute(stm.c_str());
		if (result)
		{
			del_allcolumns();
			setName(this->schema, this->name, ready);
			return result;
		}

	}

	if (col.can_null == Column::NOTNULL_DEF || col.can_null
			== Column::NOTNULL_NOTDEF)
	{
		stm = "UPDATE " + getDbfullname() + " SET " + name + " = ";
		stm += DbTable::getValue(&col);

		result = execute(stm.c_str());
		if (result)
		{
			del_allcolumns();
			setName(this->schema, this->name, ready);
			return result;
		}
	}

	stm = "ALTER TABLE " + getDbfullname() + " ALTER " + name + " ";
	if (col.can_null == Column::NULL_DEF || col.can_null == Column::NULL_NOTDEF)
		stm += "DROP NOT NULL";
	else
		stm += "SET NOT NULL";

	result = execute(stm.c_str(), ready);

	if (result)
	{
		del_allcolumns();
		setName(this->schema, this->name, ready);
		return result;
	}

	del_allcolumns();
	setName(this->schema, this->name);

	if (ready)
		end();

	del_allcolumns();
	return result;

}

int PgTable::del_column(std::string name, int ready)
{
	int result;
	std::string stm;

	if (this->name == "")
	{
		msg.perror(NO_TABLENAME, "Kein Tabellenname beim löschen vorhanden");
		if (ready)
			end();
		del_allcolumns();
		return -NO_TABLENAME;
	}

	stm = "ALTER TABLE " + getDbfullname() + " DROP " + name;

	result = execute(stm.c_str(), ready);
	del_allcolumns();
	setName(this->schema, this->name, ready);

	return result;

}

int PgTable::mv_column(std::string oldname, std::string newname, int ready)
{
	int result;
	std::string stm;

	if (this->name == "")
	{
		msg.perror(NO_TABLENAME, "Kein Tabellenname beim umbenennen vorhanden");
		if (ready)
			end();
		return -NO_TABLENAME;
	}

	stm = "ALTER TABLE " + getDbfullname() + " RENAME " + oldname + " TO "
			+ newname;

	result = execute(stm.c_str(), ready);
	del_allcolumns();
	setName(this->schema, this->name, ready);

	return result;

}

int PgTable::check_history(int ready)
{
	std::string stm;
	stm = "select mne_catalog.history_check('" + getSchema() + "','"
			+ getDbname() + "')";

	execute(stm.c_str(), ready);
	if (result.size() > 0)
		return (long) (result[0][0]);
	else
		return 0;

}

void PgTable::add_history(std::string id, int ready)
{
	std::string stm;
	stm = TAB_PRIMARY;
	execute(stm.c_str(), ready);

	if (result.size() > 0)
		id = (char*) (result[0][0]);

	if ( cols.find(id) == cols.end() )
	{
		msg.perror(E_HISTORYID, "keine Spalte <%s> vorhanden", id.c_str());
		return;
	}

	stm = "select mne_catalog.history_create('" + getSchema() + "','"
	    + getDbname() + "','" + id + "', 'mne_base.history')";

	execute(stm.c_str(), ready);
	return;

}

void PgTable::del_history(int ready)
{
	std::string stm;
	stm = "select mne_catalog.history_drop('" + getSchema() + "','"
			+ getDbname() + "')";

	execute(stm.c_str(), ready);
	return;

}

int PgTable::insert(ValueMap *c, int ready, int ignore_error)
{
	std::string stm;
	ValueMap::iterator i;
	int first;
	int result;
	char mdate[32];
	char cdate[32];

    sprintf(cdate, "%ld", time(NULL));
    memcpy(mdate, cdate, 32);

	if (this->name == "")
	{
		msg.perror(NO_TABLENAME, "Kein Tabellenname beim Einfügen in die "
			" Tabelle vorhanden");
		if (ready)
			end();
		return -NO_TABLENAME;
	}

	if (c->empty())
	{
		msg.perror(NO_COLS, "Keine Spalten zum Einfügen in die Tabelle "
			"%s vorhanden", getDbfullname().c_str());
		if (ready)
			end();
		return -NO_COLS;
	}

	result = 0;
	stm = "INSERT INTO " + this->schema + ". " + this->name + "( ";

	start_columncheck();
	if (this->schema == "pg_catalog")
		ok_columncheck("oid");

	for (first = 1, i = c->begin(); i != c->end(); ++i)
	{
		if (this->cols.find(i->first) != this->cols.end())
		{
			if (!(have_usertime_columns && (i->first == "createuser"
					|| i->first == "createdate" || i->first == "modifyuser"
					|| i->first == "modifydate")))
			{
				if (!first)
					stm += " , ";
				first = 0;

				stm += "\"" + i->first + "\"";
				ok_columncheck(i->first);
			}
		}
		else if ( ! ( i->first == "ocreatedate" || i->first == "omodifydate") )
		{
			result = -1;
			msg.perror(COL_UNKNOWN, "Spalte %s ist in der Tabelle "
				"%s beim Einfügen nicht forhanden", i->first.c_str(),
					getDbfullname().c_str());
		}
	}

	if (have_usertime_columns)
	{
		ok_columncheck("createuser");
		ok_columncheck("createdate");
		ok_columncheck("modifyuser");
		ok_columncheck("modifydate");
		stm += ", createuser, createdate, modifyuser, modifydate";
	}

	result += columncheck();

	if (result)
		return result;

	stm += " ) VALUES (";

	for (first = 1, i = c->begin(); i != c->end(); ++i)
	{
	    ColumnMap::iterator ic;

	    if (!(have_usertime_columns && (i->first == "createuser" || i->first
	            == "createdate" || i->first == "modifyuser" || i->first
	            == "modifydate")))
	    {
	        if ( ( ic = this->cols.find(i->first)) != this->cols.end() )
	        {
	            if (!first)
	                stm += " , ";
	            first = 0;

	            ic->second.value = i->second.value;
	            stm += DbTable::getValue(&(ic->second));
	        }
	        else if ( have_usertime_columns && i->first == "ocreatedate" )
	        {
	            if ( ( ic = this->cols.find("createdate")) != this->cols.end() )
	            {
	                ic->second.value = i->second.value;
	                memcpy(cdate, DbTable::getValue(&(ic->second)).c_str(), 32);
	            }
	            cdate[31] = '\0';
	        }
	        else if ( have_usertime_columns && i->first == "omodifydate" )
	        {
	            ic->second.value = i->second.value;
	            if ( ( ic = this->cols.find("modifydate")) != this->cols.end() )
	            {
                    ic->second.value = i->second.value;
	                memcpy(mdate, DbTable::getValue(&(ic->second)).c_str(), 32);
	            }
	            mdate[31] = '\0';
	        }
	    }
	}

	if (have_usertime_columns)
		stm = stm + ", session_user, " + cdate + ", session_user, " + mdate;

	stm += " ) ";

	if (ignore_error)
		setIgnore_error();
	result = execute(stm.c_str(), ready);
	resetIgnore_error();

	return result;
}

std::string PgTable::mk_where(ValueMap *w, CsList *wop)
{

	ValueMap::iterator i;
	ColumnMap::iterator ci;
	Column col;
	int wi;
	std::string op;
	std::string where(" WHERE ");
	std::string whereconcat(" AND ");

	// Where String zusammensetzen
	// ===========================

	if ( w == NULL ) return "";
	if ( w->empty() ) return "";

	for (wi = 0, i = w->begin(); i != w->end(); ++wi, ++i)
	{
		if ( i != w->begin() )
		    where += whereconcat;

		if ((ci = this->cols.find(i->first)) != this->cols.end())
		{
			col = ci->second;
			col.value = i->second.value;

			if (wop == NULL || wop->size() == 0)
				op = " = ";
			else
				op = " " + (*wop)[wi] + " ";

                        if (op.find("null") != std::string::npos || op.find("NULL") != std::string::npos )
                                where += "\"" + i->first + "\"" + op;
                        else if ((col.can_null & Column::DBNULL) && col.value == "")
				where += "\"" + i->first + "\" isnull ";
			else if (ci->second.typ == DbConnect::CHAR && op == " like ")
				where += "lower(\"" + i->first + "\")" + op + "lower("
						+ DbTable::getValue(&col) + ")";
			else
				where += "\"" + i->first + "\"" + op + DbTable::getValue(&col);
		}
		else
		{
			where += " 0 = 1 ";
			msg.perror(WHERE_COLS, "mk_where(): Spalte <%s> ist unbekannt",
					i->first.c_str());
		}

	}
	return where;
}

int PgTable::modify(ValueMap *c, ValueMap *w, int ready)
{
	std::string stm;
	ValueMap::iterator i;
	int first;
	int result;
	ColumnMap::iterator ci;
	Column col;
	std::string where;

	if (this->name == "")
	{
		msg.perror(NO_TABLENAME, "Kein Tabellenname beim Modifizieren in die "
			" Tabelle vorhanden");
		if (ready)
			end();
		return -NO_TABLENAME;
	}

	if (c->empty())
	{
		msg.perror(NO_COLS, "Keine Spalten zum Modifizieren der Tabelle "
			"%s vorhanden", getDbfullname().c_str());
		if (ready)
			end();
		return -NO_COLS;
	}

	where = mk_where(w);
	// Update zusammensetzen
	// =====================
	result = 0;
	stm = "UPDATE " + getDbfullname() + " SET ";

	for (first = 1, i = c->begin(); i != c->end(); ++i)
	{
		if ((ci = this->cols.find(i->first)) != this->cols.end())
		{
			if (!(have_usertime_columns && (i->first == "createuser"
					|| i->first == "createdate" || i->first == "modifyuser"
					|| i->first == "modifydate")))
			{
				if (!first)
					stm += " , ";
				first = 0;

				col = ci->second;
				col.value = i->second.value;
				stm += "\"" + i->first + "\" = " + DbTable::getValue(&col);
			}
		}
		else
		{
			result = -1;
			msg.perror(COL_UNKNOWN, "Spalte %s ist in der Tabelle "
				"%s beim Modifizieren nicht forhanden", i->first.c_str(),
					getDbfullname().c_str());
		}
	}

	if (have_usertime_columns)
	{
		char now[32];
		sprintf(now, "%ld", time(NULL));

		stm += std::string(", modifyuser = session_user, modifydate = ") + now;
	}

	stm += " " + where;

	if (result)
	{
		if (ready)
			end();
		return result;
	}

	return execute(stm.c_str(), ready);
}

int PgTable::del(ValueMap *w, int ready)
{
	std::string stm = "delete from " + getDbfullname() + " " + mk_where(w);
	return execute(stm.c_str(), ready);
}

DbConnect::ResultMat *PgTable::select(ValueMap *cols, ValueMap *w, CsList *wop, CsList *o, int distinct, int ready)
{
	laststm = "select ";
	ValueMap::iterator i;
	std::vector<std::string>::iterator vi;

	if (distinct)
		laststm += "distinct ";

	i = cols->begin();
	if (i != cols->end())
	{
		laststm += "\"" + i->first + "\"";
		for (++i; i != cols->end(); ++i)
			laststm += ", \"" + i->first + "\"";

		laststm += " from " + getDbfullname() + " " + mk_where(w, wop);
		if (o != NULL)
		{
            std::string str;
            std::string asc;
            for (vi = o->begin(); vi != o->end(); ++vi)
            {
            	std::string str;
            	std::string asc;
            	if ( vi == o->begin() )
            		laststm += " ORDER BY ";
            	else
            		laststm += ',';
            	str = *vi;
            	if ( str[0] == '!' )
            	{
            		str.replace(0, 1, "");
            		asc = "DESC";
            	}
            	else
            	{
            		asc = "ASC";
            	}
            	laststm += "\"" + str + "\" " + asc;
            }
		}
		execute(laststm.c_str(), ready);
	}
	else
	{
		p_get_result()->clear();
	}

	return p_get_result();
}

DbConnect::ResultMat *PgTable::select(CsList *cols, ValueMap *w, CsList *wop, CsList *o, int distinct, int ready)
{
	laststm = "select ";
	CsList::Elements::iterator i,vi;

	if (distinct)
		laststm += "distinct ";

	i = cols->begin();
	if (i != cols->end())
	{
		laststm += "\""+ (std::string) *i + "\"";
		for (++i; i != cols->end(); ++i)
			laststm += ", \"" + (std::string) *i + "\"";

		laststm += " from " + getDbfullname() + " " + mk_where(w, wop);
		if (o != NULL)
		{
            std::string str;
            std::string asc;
            for (vi = o->begin(); vi != o->end(); ++vi)
            {
            	std::string str;
            	std::string asc;
            	if ( vi == o->begin() )
            		laststm += " ORDER BY ";
            	else
            		laststm += ',';
            	str = *vi;
            	if ( str[0] == '!' )
            	{
            		str.replace(0, 1, "");
            		asc = "DESC";
            	}
            	else
            	{
            		asc = "ASC";
            	}
            	laststm += "\"" + str + "\" " + asc;
            }

		}
		execute(laststm.c_str(), ready);
	}
	else
	{
		p_get_result()->clear();
	}

	return p_get_result();
}

