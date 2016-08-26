#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <utils/tostring.h>

#include <db/dbtable.h>
#include <db/dbconstraint.h>
#include <db/dbconstraint_error.h>
#include <db/dbquery.h>

#include "dbhttpadmin_table.h"

DbHttpAdminTable::DbHttpAdminTable(DbHttp *h)
:DbHttpProvider(h),
msg("DbHttpAdminTable")
{
	subprovider["/column/add.xml"] = &DbHttpAdminTable::column_add_xml;
	subprovider["/column/mod.xml"] = &DbHttpAdminTable::column_mod_xml;
	subprovider["/column/del.xml"] = &DbHttpAdminTable::column_del_xml;

	subprovider["/table/add.xml"] = &DbHttpAdminTable::table_add_xml;
	subprovider["/table/mod.xml"] = &DbHttpAdminTable::table_mod_xml;
	subprovider["/table/del.xml"] = &DbHttpAdminTable::table_del_xml;

	subprovider["/pkey/add.xml"] = &DbHttpAdminTable::pkey_add_xml;
	subprovider["/pkey/mod.xml"] = &DbHttpAdminTable::pkey_mod_xml;
	subprovider["/pkey/del.xml"] = &DbHttpAdminTable::pkey_del_xml;

	subprovider["/check/add.xml"] = &DbHttpAdminTable::check_add_xml;
	subprovider["/check/mod.xml"] = &DbHttpAdminTable::check_mod_xml;
	subprovider["/check/del.xml"] = &DbHttpAdminTable::check_del_xml;

	subprovider["/fkey/add.xml"] = &DbHttpAdminTable::fkey_add_xml;
	subprovider["/fkey/mod.xml"] = &DbHttpAdminTable::fkey_mod_xml;
    subprovider["/fkey/del.xml"] = &DbHttpAdminTable::fkey_del_xml;

    subprovider["//conrefresh.xml"] = &DbHttpAdminTable::conrefresh;

	h->add_provider(this);
}

DbHttpAdminTable::~DbHttpAdminTable()
{
}

int DbHttpAdminTable::request(Database *db, HttpHeader *h)
{
	SubProviderMap::iterator i;

	if ( ( i = subprovider.find(h->dirname + "/" + h->filename)) != subprovider.end() )
	{
		DbTable *t = db->p_getTable(db->getApplschema(), "translate");
		(this->*(i->second))(db, h);
		t->del_allcolumns();
		db->release(t);
		return 1;
	}
	return 0;
}

DbTable::Column DbHttpAdminTable::column_par_xml( Database *db, HttpHeader *h)
{
	DbTable::Column col;

	col.typ         = atoi(h->vars["ntypInput"].c_str());
	col.size        = atoi(h->vars["maxlengthInput"].c_str());
	col.can_null    = atoi(h->vars["defaultInput"].c_str()) + (atoi(h->vars["nullableInput"].c_str()) << 1);
	col.value       = h->vars["defvalueInput"];
	col.text["de"]  = h->vars["text_deInput"];
	col.text["en"]  = h->vars["text_enInput"];

	return col;

}
void DbHttpAdminTable::column_add_xml( Database *db, HttpHeader *h)
{
	DbTable *tab;
	DbTable::Column col;

	std::string schema = h->vars["schemaInput"];
	std::string table  = h->vars["tableInput"];
	std::string column = h->vars["columnInput"];

	h->status = 200;
	h->content_type = "text/xml";


	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	tab = db->p_getTable(schema, table);
	if ( tab->getColumn(column).name != "" )
	{
		msg.perror(E_COLDUP, "Spalte <%s> ist schon vorhanden", column.c_str());
		add_content(h,  "<body>error</body>");
	}
	else
	{
		col = column_par_xml(db,h);
		if ( tab->add_column(column, col) == 0 )
		{
			DbTable *tabname = db->p_getTable(db->getApplschema(), "tablecolnames");
			DbTable::ValueMap values;

			values["schema"] = schema;
			values["tab"] = table;
			values["colname"] = column;
			values["text_de"] = col.text["de"];
                        values["text_en"] = col.text["en"];
                        values["custom"]  = h->vars["customInput"];
			values["regexp"] = h->vars["regexpInput"];
			if ( h->vars["regexphelpInput"] != "" )
			{
				values["regexphelp"] = h->vars["regexphelpInput"];
			    msg.get(h->vars["regexphelpInput"]);
			}
			values["dpytype"] = h->vars["ndpytypeInput"];
			values["showhistory"] = h->vars["showhistoryInput"];

			if ( tabname->insert(&values) == 0 )
			{
				add_content(h,  "<body>");
				add_content(h,  "<%s>%s</%s>", "schema", schema.c_str(), "schema");
				add_content(h,  "<%s>%s</%s>", "table",  table.c_str(),  "table");
				add_content(h,  "<%s>%s</%s>", "column", column.c_str(), "column");
				add_content(h,  "</body>");
			}
			else
				add_content(h,  "<body>error</body>");
			db->release(tabname);
		}
		else
			add_content(h,  "<body>error</body>");
	}

	if ( h->vars["sqlend"] != "" )
		tab->end();

	db->release(tab);
	return;
}
void DbHttpAdminTable::column_mod_xml( Database *db, HttpHeader *h)
{
	DbTable::Column col;
	DbTable *tab;

	std::string schema    = h->vars["schemaInput"];
	std::string table     = h->vars["tableInput"];
	std::string column    = h->vars["columnInput"];
	std::string columnold = h->vars["columnInput.old"];

	h->status = 200;
	h->content_type = "text/xml";


	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	tab = db->p_getTable(schema, table);
	if ( tab->getColumn(columnold).name == "" )
	{
		msg.perror(E_COLNO, "keine Spalte <%s> vorhanden", columnold.c_str());
		add_content(h,  "<body>error</body>");
	}
	else
	{
		col = column_par_xml(db,h);
		if ( tab->chk_column(columnold, col, 1) == 0 )
		{
			if ( columnold != column )
				tab->mv_column(columnold, column);

			DbTable *tabname = db->p_getTable(db->getApplschema(), "tablecolnames");
			DbTable::ValueMap values;
			DbTable::ValueMap where;

			values["schema"] = schema;
			values["tab"]    = table;
			values["colname"] = column;
			values["text_de"] = col.text["de"];
			values["text_en"] = col.text["en"];
                        values["custom"]  = h->vars["customInput"];
			values["regexp"] = h->vars["regexpInput"];
			if ( h->vars["regexphelpInput"] != "" )
			{
				values["regexphelp"] = h->vars["regexphelpInput"];
			    msg.get(h->vars["regexphelpInput"]);
			}
			values["dpytype"] = h->vars["ndpytypeInput"];
			values["showhistory"] = h->vars["showhistoryInput"];

			where["schema"] = schema;
			where["tab"]    = table;
			where["colname"] = columnold;

			if ( tabname->modify(&values, &where) == 0 )
			{
				where["colname"] = column;
				if ( tabname->select(&values, &where)->empty() )
					tabname->insert(&values);

				add_content(h,  "<body>");
				add_content(h,  "<%s>%s</%s>", "schema", schema.c_str(), "schema");
				add_content(h,  "<%s>%s</%s>", "table",  table.c_str(),  "table");
				add_content(h,  "<%s>%s</%s>", "column", column.c_str(), "column");
				add_content(h,  "</body>");
			}
			else
				add_content(h,  "<body>error</body>");

			db->release(tabname);
		}
		else
			add_content(h,  "<body>error</body>");
	}

	if ( h->vars["sqlend"] != "" )
		tab->end();

	db->release(tab);
	return;
}
void DbHttpAdminTable::column_del_xml( Database *db, HttpHeader *h)
{
	DbTable *tab;
	DbTable::Column col;

	std::string schemaold = h->vars["schemaInput.old"];
	std::string tableold  = h->vars["tableInput.old"];
	std::string columnold = h->vars["columnInput.old"];

	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	tab = db->p_getTable(schemaold, tableold);
	if ( tab->getColumn(columnold).name == "" )
	{
		msg.perror(E_COLNO, "keine Spalte <%s> vorhanden", columnold.c_str());
		add_content(h,  "<body>error</body>");
	}
	else
	{
		if ( tab->del_column(columnold) == 0 )
		{
			DbTable *tabname = db->p_getTable(db->getApplschema(), "tablecolnames");
			DbTable::ValueMap where;

			where["schema"] = schemaold;
			where["tab"]    = tableold;
			where["colname"] = columnold;

			if ( tabname->del(&where) == 0 )
				add_content(h,  "<body>ok</body>");
			else
				add_content(h,  "<body>error</body>");

			db->release(tabname);
		}
		else
			add_content(h,  "<body>error</body>");
	}

	if ( h->vars["sqlend"] != "" )
		tab->end();

	db->release(tab);
	return;
}

void DbHttpAdminTable::table_add_xml( Database *db, HttpHeader *h)
{
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	DbTable *tab;
	DbTable::ColumnMap cols;

	std::string schema = h->vars["schemaInput"];
	std::string table  = h->vars["tableInput"];

	cols[table + "id"] = DbTable::Column(table + "id", DbConnect::CHAR, 32, "", DbTable::Column::NOTNULL_NOTDEF);
	if ( h->vars["haveusertimecolumnInput"] != "" )
	{
		cols["createuser"] = DbTable::Column("createuser", DbConnect::CHAR, 32, "", DbTable::Column::NOTNULL_NOTDEF);
		cols["modifyuser"] = DbTable::Column("modifyuser", DbConnect::CHAR, 32, "", DbTable::Column::NOTNULL_NOTDEF);
		cols["createdate"] = DbTable::Column("createdate", DbConnect::LONG, "", DbTable::Column::NOTNULL_NOTDEF);
		cols["modifydate"] = DbTable::Column("modifydate", DbConnect::LONG, "", DbTable::Column::NOTNULL_NOTDEF);
	}

	tab = db->p_getTable(schema,table);
	if ( tab->create(schema, table, &cols) == 0 )
	{
		if ( h->vars["showhistoryInput"] != "" )
            	tab->add_history(table + "id");

		DbTable *tabname = db->p_getTable(db->getApplschema(), "tablecolnames");
		DbTable::ValueMap values;

		values["schema"] = schema;
		values["tab"] = table;
        values["colname"] = table + "id";
        values["custom"] = "false";

		if ( tabname->insert(&values) == 0 )
		{
			add_content(h,  "<body>");
			add_content(h,  "<%s>%s</%s>", "schema", schema.c_str(), "schema");
			add_content(h,  "<%s>%s</%s>", "table",  table.c_str(),  "table");
			add_content(h,  "</body>");
		}
		else
			add_content(h,  "<body>error</body>");
		db->release(tabname);
	}
	else
		add_content(h,  "<body>error</body>");

	if ( h->vars["sqlend"] != "" )
		tab->end();

	db->release(tab);
	return;

}
void DbHttpAdminTable::table_mod_xml( Database *db, HttpHeader *h)
{
	int result = 0;
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	DbTable *tab;
	DbTable::ColumnMap cols;

	std::string schema = h->vars["schemaInput"];
	std::string table  = h->vars["tableInput"];

	std::string schemaold = h->vars["schemaInput.old"];
	std::string tableold  = h->vars["tableInput.old"];

	tab = db->p_getTable(schemaold,tableold);
	if ( tab->check_history() )
		tab->del_history();

	if ( tableold != table )
	  if ( tab->p_getColumns()->find(tableold + "id") != tab->p_getColumns()->end() )
		  tab->mv_column(tableold + "id", table + "id");

	if ( h->vars["haveusertimecolumnInput"] != "" )
	{
		result += tab->chk_column("createuser", DbTable::Column("createuser", DbConnect::CHAR, 32, "", DbTable::Column::NOTNULL_NOTDEF),1);
		result += tab->chk_column("modifyuser", DbTable::Column("modifyuser", DbConnect::CHAR, 32, "", DbTable::Column::NOTNULL_NOTDEF),1);
		result += tab->chk_column("createdate", DbTable::Column("createdate", DbConnect::LONG, "", DbTable::Column::NOTNULL_NOTDEF),1);
		result += tab->chk_column("modifydate", DbTable::Column("modifydate", DbConnect::LONG, "", DbTable::Column::NOTNULL_NOTDEF),1);
	}

	if ( result == 0 && ( schemaold != schema || tableold != table ))
	{
		if ( tab->check_history() ) tab->del_history();
		result = tab->rename(schema, table);
	}

	if ( result == 0 )
	{
		DbTable *tabname = db->p_getTable(db->getApplschema(), "tablecolnames");
		DbTable::ValueMap values;
		DbTable::ValueMap where;

		values["schema"] = schema;
		values["tab"] = table;

		where["schema"] = schemaold;
		where["tab"] = tableold;

		if ( tabname->modify(&values, &where) == 0 )
		{
			values.clear();
			values["colname"] = table + "id";

			where["schema"] = schema;
			where["tab"] = table;
			where["colname"] = tableold + "id";

			if ( tabname->modify(&values, &where) == 0 )
			{
			add_content(h,  "<body>");
			add_content(h,  "<%s>%s</%s>", "schema", schema.c_str(), "schema");
			add_content(h,  "<%s>%s</%s>", "table",  table.c_str(),  "table");
			add_content(h,  "</body>");
			}
			else
				add_content(h,  "<body>error</body>");
		}
		else
			add_content(h,  "<body>error</body>");
		db->release(tabname);
	}
	else
		add_content(h,  "<body>error</body>");

	if ( result == 0 && h->vars["showhistoryInput"] != "" )
		tab->add_history(table + "id");
	else if ( tab->check_history() )
        tab->del_history();

	if ( h->vars["sqlend"] != "" )
		tab->end();

	db->release(tab);
	return;
}

void DbHttpAdminTable::table_del_xml( Database *db, HttpHeader *h)
{
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());
	DbTable *tab;
	DbTable::ColumnMap cols;

	std::string schema = h->vars["schemaInput.old"];
	std::string table  = h->vars["tableInput.old"];

	DbTable::ValueMap where;

	where["schema"] = schema;
	where["tab"]    = table;

	tab = db->p_getTable(db->getApplschema(),"tablecolnames");
	tab->del(&where);
	db->release(tab);

	tab = db->p_getTable(schema, table);
	if ( tab->check_history() ) tab->del_history();

	if ( tab->remove(schema + "." + table) == 0 )
		add_content(h,  "<body>ok</body>");
	else
		add_content(h,  "<body>error</body>");

	if ( h->vars["sqlend"] != "" )
		tab->end();

	db->release(tab);
	return;

}

void DbHttpAdminTable::pkey_add_xml( Database *db, HttpHeader *h)
{
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	DbConstraint *con;

	std::string schema = h->vars["schema"];
	std::string table  = h->vars["table"];
	std::string name   = h->vars["nameInput"];
	std::string cols   = h->vars["colsInput"];

	std::string text_de = h->vars["text_deInput"];
    std::string text_en = h->vars["text_enInput"];
    int custom          = h->vars["customInput"] != "";

	con = db->p_getConstraint();
	if ( con->create_primary(schema, table, name, cols, text_de, text_en, custom) == 0 )
	{
	    add_content(h,  "<body><name>%s</name></body>", name.c_str());
	    DbConstraintError e;
	    e.read(db, 0);
	}
	else
		add_content(h, "<body>error</body>");

	if ( h->vars["sqlend"] != "")
		db->p_getConnect()->end();

	db->release(con);
	return;
}
void DbHttpAdminTable::pkey_mod_xml( Database *db, HttpHeader *h)
{
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	DbConstraint *con;

	std::string schema = h->vars["schema"];
	std::string table  = h->vars["table"];
	std::string name   = h->vars["nameInput"];
	std::string cols   = h->vars["colsInput"];
	std::string nameold   = h->vars["nameInput.old"];

	std::string text_de = h->vars["text_deInput"];
    std::string text_en = h->vars["text_enInput"];
    int custom          = h->vars["customInput"] != "";

    con = db->p_getConstraint();
    if ( con->remove(schema, table, nameold) == 0 )
    {
        if ( con->create_primary(schema, table, name, cols, text_de, text_en, custom) == 0 )
        {

            add_content(h,  "<body><name>%s</name></body>", name.c_str());
            DbConstraintError e;
            e.read(db, 0);
        }
        else
            add_content(h, "<body>error</body>");
    }
    else
        add_content(h, "<body>error</body>");

    if ( h->vars["sqlend"] != "")
        db->p_getConnect()->end();

	db->release(con);
	return;
}

void DbHttpAdminTable::pkey_del_xml( Database *db, HttpHeader *h)
{
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	DbConstraint *con;
	DbTable::ColumnMap cols;

	std::string schema = h->vars["schema"];
	std::string table  = h->vars["table"];
	std::string nameold   = h->vars["nameInput.old"];

	con = db->p_getConstraint();
	if ( con->remove(schema, table, nameold) == 0 )
	{
        DbConstraintError e;
        e.read(db, 0);
        add_content(h, "<body>ok</body>");
	}
	else
		add_content(h, "<body>error</body>");

	if ( h->vars["sqlend"] != "")
		db->p_getConnect()->end();

	db->release(con);
	return;
}
void DbHttpAdminTable::check_add_xml( Database *db, HttpHeader *h)
{
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	DbConstraint *con;

	std::string schema  = h->vars["schemaInput"];
	std::string table   = h->vars["tableInput"];
	std::string name    = h->vars["nameInput"];
    std::string check   = h->vars["checkInput"];
    std::string text_de = h->vars["text_deInput"];
    std::string text_en = h->vars["text_enInput"];
    int custom          = h->vars["customInput"] != "";

	con = db->p_getConstraint();
	if ( con->create_check(schema, table, name, check, text_de, text_en, custom) == 0 )
	{
	    add_content(h,  "<body><name>%s</name></body>", name.c_str());
        DbConstraintError e;
        e.read(db, 0);
	}
	else
		add_content(h, "<body>error</body>");

	if ( h->vars["sqlend"] != "")
		db->p_getConnect()->end();

	db->release(con);
	return;
}
void DbHttpAdminTable::check_mod_xml( Database *db, HttpHeader *h)
{
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	DbConstraint *con;

	std::string schema = h->vars["schemaInput"];
	std::string table  = h->vars["tableInput"];
	std::string name   = h->vars["nameInput"];
	std::string check   = h->vars["checkInput"];
	std::string nameold   = h->vars["nameInput.old"];
    std::string text_de = h->vars["text_deInput"];
    std::string text_en = h->vars["text_enInput"];
    int custom          = h->vars["customInput"] != "";

	con = db->p_getConstraint();
	if ( con->remove(schema, table, nameold) == 0 )
	{
	    if ( con->create_check(schema, table, name, check, text_de, text_en, custom) == 0 )
		{
		    add_content(h,  "<body><name>%s</name></body>", name.c_str());
            DbConstraintError e;
            e.read(db, 0);
		}
		else
			add_content(h, "<body>error</body>");
    }
	else
		add_content(h, "<body>error</body>");

	if ( h->vars["sqlend"] != "")
		db->p_getConnect()->end();

	db->release(con);
	return;
}

void DbHttpAdminTable::check_del_xml( Database *db, HttpHeader *h)
{
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	DbConstraint *con;
	DbTable::ColumnMap cols;

	std::string schema = h->vars["schemaInput.old"];
	std::string table  = h->vars["tableInput.old"];
	std::string nameold   = h->vars["nameInput.old"];

	con = db->p_getConstraint();
	if ( con->remove(schema, table, nameold) == 0 )
	{
	    add_content(h, "<body>ok</body>");
        DbConstraintError e;
        e.read(db, 0);
	}
	else
		add_content(h, "<body>error</body>");

	if ( h->vars["sqlend"] != "")
		db->p_getConnect()->end();

	db->release(con);
	return;
}
void DbHttpAdminTable::fkey_add_xml( Database *db, HttpHeader *h)
{
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	DbConstraint *con;

	std::string name   = h->vars["nameInput"];

	std::string schema = h->vars["schemaInput"];
	std::string table  = h->vars["tableInput"];
	std::string cols   = h->vars["colsInput"];

	std::string rschema = h->vars["rschemaInput"];
	std::string rtable  = h->vars["rtableInput"];
	std::string rcols   = h->vars["rcolsInput"];

    std::string text_de = h->vars["text_deInput"];
    std::string text_en = h->vars["text_enInput"];
    int custom          = h->vars["customInput"] != "";

    con = db->p_getConstraint();
	if ( con->create_foreign(schema, table, name, cols, rschema, rtable, rcols, text_de, text_en, custom) == 0 )
	{
	    add_content(h,  "<body><name>%s</name></body>", name.c_str());
        DbConstraintError e;
        e.read(db, 0);
	}
	else
		add_content(h, "<body>error</body>");

	if ( h->vars["sqlend"] != "")
		db->p_getConnect()->end();

	db->release(con);
	return;
}
void DbHttpAdminTable::fkey_mod_xml( Database *db, HttpHeader *h)
{
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	DbConstraint *con;

	std::string name   = h->vars["nameInput"];
	std::string nameold   = h->vars["nameInput.old"];

	std::string schema = h->vars["schemaInput"];
	std::string table  = h->vars["tableInput"];
	std::string cols   = h->vars["colsInput"];

	std::string rschema = h->vars["rschemaInput"];
	std::string rtable  = h->vars["rtableInput"];
	std::string rcols   = h->vars["rcolsInput"];

    std::string text_de = h->vars["text_deInput"];
    std::string text_en = h->vars["text_enInput"];
    int custom          = h->vars["customInput"] != "";

    con = db->p_getConstraint();
	if ( con->remove(schema, table, nameold) == 0 )
	{
	    if ( con->create_foreign(schema, table, name, cols, rschema, rtable, rcols, text_de, text_en, custom) == 0 )
		{
		    add_content(h,  "<body><name>%s</name></body>", name.c_str());
            DbConstraintError e;
            e.read(db, 0);
		}
		else
			add_content(h, "<body>error</body>");
    }
	else
		add_content(h, "<body>error</body>");

	if ( h->vars["sqlend"] != "")
		db->p_getConnect()->end();

	db->release(con);
	return;
}

void DbHttpAdminTable::fkey_del_xml( Database *db, HttpHeader *h)
{
	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	DbConstraint *con;
	DbTable::ColumnMap cols;

	std::string schema = h->vars["schemaInput.old"];
	std::string table  = h->vars["tableInput.old"];
	std::string nameold   = h->vars["nameInput.old"];

	con = db->p_getConstraint();
	if ( con->remove(schema, table, nameold) == 0 )
	{
	    add_content(h, "<body>ok</body>");
        DbConstraintError e;
        e.read(db, 0);
	}
	else
		add_content(h, "<body>error</body>");

	if ( h->vars["sqlend"] != "")
		db->p_getConnect()->end();

	db->release(con);
	return;
}

void DbHttpAdminTable::conrefresh(Database *db, HttpHeader *h)
{
    h->status = 200;
    h->content_type = "text/xml";

    add_content(h, 
            "<?xml version=\"1.0\" encoding=\"%s\"?><result>",
            h->charset.c_str());
    add_content(h, "<body>ok</body>");
    DbConstraintError e;
    e.read(db, h->vars["sqlend"] != "");
    return;
}

