#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <argument/argument.h>

#include "database.h"

#include "dbconnect.h"
#include "dbcursor.h"
#include "dbtable.h"
#include "dbjoin.h"
#include "dbquery.h"
#include "dbquery_creator.h"
#include "dbconstraint.h"

Database::Database() :
    msg("Database")
{
    Argument a;
    con = NULL;
    this->appl_schema = (char*)a["DbApplSchema"];
}

Database::Database(std::string appl_schema) :
          msg("Database")
{
	con = NULL;
	this->appl_schema = appl_schema;
}

Database::~Database()
{
	{
		std::set<DbQueryCreator *>::iterator v;
		for (v = querycreators.begin(); v != querycreators.end(); ++v)
			delete (*v);
	}

	{
		std::set<DbQuery *>::iterator v;
		for (v = querys.begin(); v != querys.end(); ++v)
			delete (*v);
	}

	{
		std::set<DbJoin *>::iterator j;
	for (j = joins.begin(); j != joins.end(); ++j)
		delete (*j);

	}

	{
		std::set<DbConstraint *>::iterator c;
		for (c = constraints.begin(); c != constraints.end(); ++c)
			delete (*c);
	}

	std::set<DbCursor *>::iterator c;
	for (c = curs.begin(); c != curs.end(); ++c)
		delete (*c);

	std::map<DbTable *, int>::iterator t;
	for (t = tabs.begin(); t != tabs.end(); ++t)
		delete (t->first);

	if (con != NULL)
		delete con;
}

int Database::have_connection()
{
	return (con != NULL && con->have_connection());
}


void Database::release(DbCursor *cur)
{
	std::set<DbCursor *>::iterator i;

	if (cur == last_cur)
		last_cur = NULL;

	if ((i = curs.find(cur)) != curs.end())
	{
		curs.erase(i);
		delete cur;
	}
	else
	{
		msg.perror(E_RELEASE, "kann DbUser nicht finden");
	}

	return;
}

int Database::release(DbTable *tab)
{
	std::map<DbTable *, int>::iterator i;
	int result = 0;

	if (tab == last_tab)
		last_tab = NULL;

	i = tabs.find(tab);
	if (i != tabs.end())
	{
		if (i->second > 0)
		{
			result = 1;
			i->second = i->second - 1;
			if (i->second == 0)
			{
				// Tabelle löschen falls sie doppelt
				// =================================
				for (i = tabs.begin(); i != tabs.end(); ++i)
					if (i->first != tab && i->first->getSchema()
							== tab->getSchema() && i->first->getName()
							== tab->getName())
						break;
				if (i != tabs.end())
				{
					tabs.erase(tab);
					delete tab;
				}
			}
		}
	}

	if (!result)
		msg.perror(E_RELEASE, "Tabelle %x ist unbekannt und "
			"kann nicht freigegeben werden", tab);

	return result;
}

void Database::release(DbConstraint *constraint)
{
	std::set<DbConstraint*>::iterator i;

	if (constraint == last_constraint)
		last_constraint = NULL;
	if ((i = constraints.find(constraint)) != constraints.end())
	{
		constraints.erase(i);
		delete constraint;
	}
	else
	{
		msg.perror(E_RELEASE, "kann DbConstraint nicht finden");
	}
	return;
}

void Database::release(DbJoin *join)
{
	std::set<DbJoin *>::iterator i;

	if (join == last_join)
		last_join = NULL;

	if ((i = joins.find(join)) != joins.end())
	{
		joins.erase(i);
		delete join;
	}
	else
	{
		msg.perror(E_RELEASE, "kann DbJoin nicht finden");
	}

	return;
}

DbQueryCreator *Database::p_getQuerycreator()
{
	last_querycreator = new DbQueryCreator(this);
	querycreators.insert(last_querycreator);
	return last_querycreator;
}

void Database::release(DbQueryCreator *query)
{
	std::set<DbQueryCreator *>::iterator i;

	if (query == last_querycreator)
		last_querycreator = NULL;
	if ((i = querycreators.find(query)) != querycreators.end())
	{
		querycreators.erase(i);
		delete query;
	}
	else
	{
		msg.perror(E_RELEASE, "kann DbQueryCreator nicht finden");
	}
	return;
}

DbQuery *Database::p_getQuery()
{
	last_query = new DbQuery(this);
	querys.insert(last_query);
	return last_query;
}

void Database::release(DbQuery *query)
{
	std::set<DbQuery*>::iterator i;

	if (query == last_query)
		last_query = NULL;
	if ((i = querys.find(query)) != querys.end())
	{
		querys.erase(i);
		delete query;
	}
	else
	{
		msg.perror(E_RELEASE, "kann DbQuery nicht finden");
	}
	return;
}

