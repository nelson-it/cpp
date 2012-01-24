#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include "pgjoin.h"

PgJoin::PgJoin() :
    msg("PgJoin")
{
    tabnum = 0;
    joinnum = 0;
    backid = 0;
    where_clause = "";
}

std::string PgJoin::add_table(std::string name, std::string pcols,
        std::string tcols, std::string op, int typ)
{

    if (joins.empty())
    {
        _Join j;

        if (pcols != "" || tcols != "") msg.pwarning(NOEMPTY_COLS,
                "Erste Tabelle %s benötigt keine Spaltenangabe", name.c_str());

        j.push_back(name + " t0");
        joins.push_back(j);
        deep_tabs.push_back(0);

    }
    else
    {
        std::string stm, p, t, o;
        std::string::size_type i, j, k, first;
        std::string tn, pn;
        std::string tc, pc;
        char str[16];

        if (pcols == "" && tcols == "" && op == "") msg.pwarning(EMPTY_COLS,
                "Zum Join von Tabelle %s werden "
                    "Spalten- und Operatorangaben "
                    "benötigt", name.c_str());

        switch (typ)
        {
        case INNER:
            stm = "INNER ";
            break;
        case LEFT:
            stm = "LEFT ";
            break;
        case RIGHT:
            stm = "RIGHT ";
            break;
        case FULL:
            stm = "FULL ";
            break;
        default:
            break;
        };

        if (joins[joinnum].size() == 0)
        {
            sprintf(str, "t%d", backid);
            pn = str;
            tabnum++;
        }
        else
        {
            sprintf(str, "t%d", tabnum++);
            pn = str;
        }
        sprintf(str, "t%d", tabnum);
        tn = str;

        stm += "JOIN " + name + " " + tn + " ON ( ";

        if (pcols == "" && tcols == "")
        {
            while ((i = op.find("#0")) != std::string::npos)
                op.replace(i, 2, pn);
            while ((i = op.find("#1")) != std::string::npos)
                op.replace(i, 2, tn);
            stm += op + " )";
        }
        else
        {
            p = pcols;
            t = tcols;
            o = op;
            i = p.find_first_of(',');
            j = t.find_first_of(',');
            k = o.find_first_of(',');
            first = 1;
            while (i != std::string::npos && j != std::string::npos)
            {
                pc = p.substr(0, i);
                if (pc.find('.') == std::string::npos) pc = pn + "." + pc;

                tc = t.substr(0, j);
                if (tc.find('.') == std::string::npos) tc = tn + "." + tc;

                if (!first) stm += "AND ";
                stm += pc + " " + o.substr(0, k) + " " + tc + " ";

                p = p.substr(i + 1);
                t = t.substr(j + 1);
                o = o.substr(k + 1);

                i = p.find_first_of(',');
                j = t.find_first_of(',');
                k = o.find_first_of(',');

                first = 0;
            }

            if (i != j || i != k)
            {
                msg.perror(NUM_COLS, "Die anzahl der Spalten %s:%s zum Join "
                    "von Tabelle %s sind ungleich", pcols.c_str(),
                        tcols.c_str(), name.c_str());
                return "";
            }

            pc = p;
            if (pc.find('.') == std::string::npos) pc = pn + "." + pc;

            tc = t;
            if (tc.find('.') == std::string::npos) tc = tn + "." + tc;

            if (first) stm += pc + " " + o + " " + tc + ")";
            else stm += "AND " + pc + " " + o + " " + tc + ")";

        }
        joins[joinnum].push_back(stm);
        deep_tabs.push_back(tabnum);
    }

    return getActtable();
}

void PgJoin::back(int deep)
{
    if (deep >= (int) deep_tabs.size()) return;

    joinnum++;
    _Join j;
    joins.push_back(j);

    backid = deep_tabs[deep - 1];
    deep_tabs.resize(deep);
}

std::string PgJoin::getActtable()
{
    char str[16];
    sprintf(str, "t%d", tabnum);
    return str;
}

void PgJoin::add_where(int notop, int leftbrace, std::string lefttab,
        std::string leftval, std::string op, std::string righttab,
        std::string rightval, int rightbrace, std::string boolop)
{
    if (notop) where_clause += " NOT ";

    if (leftbrace) where_clause += " (";

    if (lefttab != "") where_clause += (" " + lefttab + ".");

    where_clause += (leftval + " " + op + " ");

    if (op != "isnull" && op != "notnull")
    {
        if (righttab != "") where_clause += (righttab + ".");
        if (rightval != "") where_clause += rightval;
    }

    if (rightbrace) where_clause += ")\n";

    where_clause += " " + boolop + " ";
}

std::string PgJoin::getFrom()
{
    unsigned int i, j, k;
    std::string str;

    if (joins.size() == 1 && joins[0].size() == 1) return joins[0][0];

    for (j = 0; j < joins.size(); ++j)
        str += "(";

    for (j = 0, i = 0; j < joins.size(); ++j, i = 0)
    {
        if (i == 0)
        {
            str += joins[j][i++];
        }

        k = joins[j].size() - 1;
        for (; i < k; ++i)
            str = "(" + str + " " + joins[j][i] + " )\n";

        if ( k != 0 )
            str = "(" + str + " " + joins[j][i] + " )";

        str += ")\n";
    }

    return str;

}

