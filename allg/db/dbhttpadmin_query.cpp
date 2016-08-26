#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <utils/tostring.h>
#include <db/dbquery_creator.h>
#include "dbhttpadmin_query.h"

#ifdef PTHREAD
#endif

DbHttpAdminQuery::DbHttpAdminQuery(DbHttp *h) :
	DbHttpProvider(h), msg("DbHttpAdminQuery")
{
	subprovider["ok.xml"] = &DbHttpAdminQuery::ok;
	subprovider["del.xml"] = &DbHttpAdminQuery::del;

	h->add_provider(this);
}

DbHttpAdminQuery::~DbHttpAdminQuery()
{
}

int DbHttpAdminQuery::request(Database *db, HttpHeader *h)
{

	SubProviderMap::iterator i;

	if ((i = subprovider.find(h->filename)) != subprovider.end())
	{
		(this ->* (i->second))(db, h);
		if (h->vars["sqlend"] != "")
			db->p_getConnect()->end();

		return 1;
	}

	return 0;

}

void DbHttpAdminQuery::ok(Database *db, HttpHeader *h)
{
	DbQueryCreator *query;
	std::string queryid;
	long i, janzahl, sanzahl, wanzahl;
	int unionnum;
    int result;

	queryid = h->vars["queryidInput"];

	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	unionnum = atoi(h->vars["unionnumInput"].c_str());

	db->p_getConnect()->start();
	query = db->p_getQuerycreator();

	query->setName(h->vars["schemaInput.old"],
                   h->vars["queryInput.old"],
                   h->vars["unionnumInput.old"]);

	if (h->vars["tcolumnlength"] != "" || h->vars["twherelength"] != "")
	{
		janzahl = atol(h->vars["janzahl"].c_str());
		query->clear_join();
		for (i = 0; i < janzahl; ++i)
		{
			char deep[64];
			char tabnum[64];
			char joindefid[64];
			char fcols[64];
			char tschema[64];
			char ttab[64];
			char tcols[64];
			char op[64];
			char typ[64];

			snprintf(deep, 64, "joindeep%ld", i);
			snprintf(tabnum, 64, "jointabnum%ld", i);
			snprintf(joindefid, 64, "joinjoindefid%ld", i);
			snprintf(fcols, 64, "joinfcols%ld", i);
			snprintf(tschema, 64, "jointschema%ld", i);
			snprintf(ttab, 64, "jointtab%ld", i);
			snprintf(tcols, 64, "jointcols%ld", i);
			snprintf(op, 64, "joinop%ld", i);
			snprintf(typ, 64, "jointyp%ld", i);

			if (query->add_join(atoi(h->vars[deep].c_str()), atoi(h->vars[tabnum].c_str()),
					h->vars[joindefid], h->vars[fcols], h->vars[tschema],
					h->vars[ttab], h->vars[tcols], h->vars[op], atoi(
							h->vars[typ].c_str())) < 0)
			{
				msg.perror(E_JADD, "Fehler beim hinzufügen eines Joins");
				add_content(h,  "<body>error</body>");
				return;
			}

		}
	}

	if (h->vars["tcolumnlength"] != "")
	{
		sanzahl = atol(h->vars["tcolumnlength"].c_str());

		query->clear_select();
		for (i = 0; i < sanzahl; i++)
		{
			char t[32];
			char n[32];
			char c[32];
			char l[32];
			char ty[32];
			char f[32];
            char g[32];
            char hm[32];
			char de[32];
			char en[32];
			std::string col;
			std::string lang;
			std::string::size_type j;

			snprintf(t, 32, "jtabnumInput%ld", i);
			t[31] = '\0';
			snprintf(n, 32, "colInput%ld", i);
			n[31] = '\0';
			snprintf(c, 32, "colidInput%ld", i);
			c[31] = '\0';
			snprintf(l, 32, "langInput%ld", i);
			l[31] = '\0';
			snprintf(ty, 32, "typInput%ld", i);
			ty[31] = '\0';
			snprintf(f, 32, "formatInput%ld", i);
			f[31] = '\0';
            snprintf(g, 32, "groupbyInput%ld", i);
            g[31] = '\0';
            snprintf(hm, 32, "musthavingInput%ld", i);
            g[31] = '\0';
			snprintf(de, 32, "colname_deInput%ld", i);
			de[31] = '\0';
			snprintf(en, 32, "colname_enInput%ld", i);
			en[31] = '\0';

			col = h->vars[n];
			if (atoi(h->vars[l].c_str()) != 0)
			{
				lang = msg.getLang();
				if ((j = col.rfind("_" + lang)) != std::string::npos)
					if (j == col.rfind("_"))
						col = col.erase(j);
			}
			else
			{
				lang = "";
			}

			if (query->add_select(h->vars[t], col, h->vars[c], lang, atoi(
					h->vars[ty].c_str()), h->vars[f], atoi(h->vars[g].c_str()), atoi(h->vars[hm].c_str()),
					h->vars[de], h->vars[en]) < 0)
			{
				msg.perror(E_SADD,
						"Tabelle <%s> oder Spalte <%s> beim addieren "
							"zum Selektieren nicht vorhanden",
						h->vars[t].c_str(), h->vars[n].c_str());
				add_content(h,  "<body>error</body>");
				return;
			}
		}
	}

	if (h->vars["twherelength"] != "")
	{
		wanzahl = atol(h->vars["twherelength"].c_str());
		query->clear_where();
		for (i = 0; i < wanzahl; i++)
		{
			char notop[32];
			char leftbrace[32];
			char lefttab[32];
			char leftval[32];
			char op[32];
			char righttab[32];
			char rightval[32];
			char rightbrace[32];
			char boolop[32];

			snprintf(notop, 32, "wnotInput%ld", i);
			notop[31] = '\0';
			snprintf(leftbrace, 32, "wleftbraceInput%ld", i);
			leftbrace[31] = '\0';
			snprintf(lefttab, 32, "wlefttabInput%ld", i);
			lefttab[31] = '\0';
			snprintf(leftval, 32, "wleftvalInput%ld", i);
			leftval[31] = '\0';
			snprintf(op, 32, "wopInput%ld", i);
			op[31] = '\0';
			snprintf(righttab, 32, "wrighttabInput%ld", i);
			righttab[31] = '\0';
			snprintf(rightval, 32, "wrightvalInput%ld", i);
			rightval[31] = '\0';
			snprintf(rightbrace, 32, "wrightbraceInput%ld", i);
			rightbrace[31] = '\0';
			snprintf(boolop, 32, "wboolopInput%ld", i);
			boolop[31] = '\0';

			if (query->add_where(h->vars[notop], h->vars[leftbrace],
					h->vars[lefttab], h->vars[leftval], h->vars[op],
					h->vars[righttab], h->vars[rightval], h->vars[rightbrace],
					h->vars[boolop]) < 0)
			{
				msg.perror(E_WADD, "Table <%s,%s> oder Spalte <%s,%s> beim "
					"addieren zur Whereclause nicht vorhanden",
						h->vars[lefttab].c_str(), h->vars[righttab].c_str(),
						h->vars[leftval].c_str(), h->vars[rightval].c_str());
				add_content(h,  "<body>error</body>");
				return;
			}
		}
	}

	result = query->save(h->vars["schemaInput"],
			             h->vars["queryInput"],
			             unionnum,
			             h->vars["queryidInput"],
                         atoi(h->vars["unionallInput"].c_str()) != 0, atoi(
					     h->vars["selectdistinctInput"].c_str()) !=0,
					     h->vars["copy"] != "");

	if ( result >= 0  )
		add_content(h,  "<body><queryid>%s</queryid></body>",query->getQueryid().c_str());
	else
		add_content(h,  "<body>error</body>");

	db->release(query);
	return;
}

void DbHttpAdminQuery::del(Database *db, HttpHeader *h)
{
	DbQueryCreator *query;
	std::string queryid;

	h->status = 200;
	h->content_type = "text/xml";

	add_content(h, 
			"<?xml version=\"1.0\" encoding=\"%s\"?><result>",
			h->charset.c_str());

	queryid = h->vars["queryidInput.old"];

	query = db->p_getQuerycreator();
	if ( query->del(queryid) == 0 )
		add_content(h,  "<body>ok</body>");
	else
		add_content(h,  "<body>error</body>");

    db->release(query);

    return;
}
