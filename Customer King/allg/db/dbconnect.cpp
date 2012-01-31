#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sstream>

#ifdef Darwin
#include <xlocale.h>
#endif

#ifdef __MINGW32__
#include <winsock2.h>
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include "dbconnect.h"
#include "dbtable.h"

DbConnect::Result::Result()
{
    typ = DbConnect::UNKNOWN;
    value = NULL;
    length = 0;
    isnull = 0;
}

DbConnect::Result::Result(const Result &in)
{
    typ = in.typ;
    length = in.length;
    value = in.value;
    isnull = in.isnull;

    if (value != NULL)
    {
        value = new char[in.length];
        memcpy(value, in.value, in.length);
    }
}

DbConnect::Result& DbConnect::Result::operator=(const DbConnect::Result &in)
{
    typ = in.typ;
    length = in.length;
    value = in.value;
    isnull = in.isnull;

    if ( value != NULL )
    {
        value = new char[in.length];
        memcpy(value, in.value, in.length);
    }

    return *this;
}

char *DbConnect::Result::format(Message *msg, char *str, int length, const char *format)
{
    char *val;
    unsigned int l;
    char c[1024];

    if (str == NULL || length < 1)
    {
        val = c;
        *c = '\0';
        l = 1023;
    }
    else
    {
        val = str;
        l = length - 1;
    }

    val[l] = '\0';

    if (isnull)
    {
        val[0] = '\0';
        return val;
    }

    switch (typ)
    {
    case BOOL:
        if (*(long *) value != 0)
        {
            strncpy(val, "true", l);
        }
        else
        {
            strncpy(val, "false", l);
        }
        break;

    case CHAR:
        if (format != NULL && *format == '\'' && *(char *) value == '\0')
            return ((char*)"''");

        if (str == NULL)
            val = (char *) value;
        else
            strncpy(val, (char *) value, l);
        break;

    case SHORT:
    case LONG:
        if (format != NULL && *format == 't')
            snprintf(val, l, "%s", ctime((long*) value));
        else
            snprintf(val, l, "%ld", *(long*) value);
        break;

    case FLOAT:
    case DOUBLE:
    {
        char f[128];
        if (format != NULL && *format != '\'' && *format != '\0')
            strcpy(f, format);
        else
            strcpy(f,"%'f");

        snprintf(val, l, f, *(double*) value);
        break;
    }

    default:
        if (str == NULL)
        {
            if (msg != NULL)
                strncpy(val, msg->get("unbekannter Typ").c_str(), l);
            else
                strncpy(val, "unbekannter Typ", l);
        }
        break;
    }

    return val;
}

DbConnect::Result::~Result()
{
    switch (typ)
    {
    case DbConnect::CHAR:
        delete[] (char*) value;
        break;

    case DbConnect::BOOL:
    case DbConnect::SHORT:
    case DbConnect::LONG:
        delete (long*) value;
        break;

    case DbConnect::DOUBLE:
    case DbConnect::FLOAT:
        delete (double*) value;
        break;

    default:
        fprintf(stderr, "Type %d im Resultdestructor nicht bekannt\n", typ);
        exit(99);
        break;
    }
}

void DbConnect::mk_string(std::string &str, int nodelimter)
{
    std::string::iterator i;
    if ( str.find("E'") == 0 )
    {
        msg.pdebug(0, "Wahrscheinlich doppelter Aufruf von mk_string für <%s>", str.c_str());
        return;
    }

    for (i = str.begin(); i != str.end(); ++i)
    {
        if (*i == '\'')
        {
            i = str.insert(i, '\'');
            ++i;
        }
        if (*i == '\\')
        {
            i = str.insert(i, '\\');
            ++i;
        }
    }
    if (!nodelimter)
    {
        str.insert(str.begin(), '\'');
        str.push_back('\'');
    }

    str.insert(str.begin(), 'E');
}

std::string DbConnect::getValue(int typ, std::string value)
{
    std::string str;

    if (typ == DbConnect::CHAR && value == "################")
    {
        str = "'" + mk_index() + "'";
    }
    else if (typ == DbConnect::CHAR || typ == DbConnect::EMAIL || typ == DbConnect::LINK )
    {
        str = value;
        mk_string(str);
    }
    else if ( value == "null")
    {
        return value;
    }
    else if ( typ == DbConnect::SHORT || typ == DbConnect::LONG )
    {
        long v = 0;
        std::string::size_type i;
        char dval[1024];

        struct lconv *l;
        l = localeconv();

        i = 0;
        while ( *(l->thousands_sep) != '\0' && (i = value.find(l->thousands_sep, i) ) != std::string::npos)
            value.replace(i, 1, "");

        sscanf(value.c_str(), "%ld", &v);

        locale_t loc;
        loc = newlocale(LC_NUMERIC_MASK, "C", NULL);
        loc = uselocale(loc);

        sprintf(dval, "%20ld", v);

        loc = uselocale(loc);
        freelocale(loc);
        return dval;
    }
    else if ( typ == DbConnect::DOUBLE || typ == DbConnect::FLOAT )
    {
        double v = 0;
        std::string::size_type i;
        char fval[1024];

        struct lconv *l;
        l = localeconv();

        i = 0;
        while ( *(l->thousands_sep) != '\0' && (i = value.find(l->thousands_sep, i) ) != std::string::npos)
            value.replace(i, 1, "");

        sscanf(value.c_str(), "%lf", &v);

        locale_t loc;
        loc = newlocale(LC_NUMERIC_MASK, "C", NULL);
        loc = uselocale(loc);

        sprintf(fval, "%40.40f", v);

        loc = uselocale(loc);
        freelocale(loc);
        return fval;
    }
    else if (typ == DbConnect::BOOL)
    {
        if (value == "true" || value == "t" || value == "r" || value
                == "richtig" || atoi(value.c_str()) != 0)
            str = "true";
        else
            str = "false";
    }
    else if (typ != DbConnect::CHAR && value == "")
    {
        str = "0";
    }
    else if (value == "true")
    {
        str = "1";
    }
    else if (value == "false")
    {
        str = "0";
    }
    else
    {
        return value;
    }

    return str;
}

std::string DbConnect::mk_index()
{
    char str[33];
    static int indexcount = 0;
    static time_t act_time;
    time_t t;

    if ((t = time(NULL)) != act_time)
    {
        indexcount = 0;
        act_time = t;
    }
    else if (indexcount == 0x7fff)
    {
        while ((t = time(NULL)) == act_time)
        {
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            select(0, NULL, NULL, NULL, &tv);
        }
        act_time = t;
        indexcount = 0;
    }
    else
        indexcount++;

    snprintf(str, 32, "%08lx%04x", time(NULL), indexcount);
    str[32] = '\0';
    return str;
}
