#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(__CYGWIN__) || defined(__MINGW32__)
#include <windows.h>
#endif

#include "argument.h"

Argument::Element Argument::operator[](const char *name)
{

    return Element(name, this);
}

Argument::Element::Element(const char *name, Argument *a)
{
    this->name = name;
    this->a = a;
}

Argument::StringWerte Argument::Element::getStringWerte()
{
    Argument::StringWerteMap::iterator i;

    if ((i = a->sval.find(name)) != a->sval.end()) return i->second;
    else
    {
        a->msg.perror(Argument::NO_STRINGARG, "%s ist keinen Zeichenkettenargument", name);
        return StringWerte();
    }
}

Argument::DoubleWerte Argument::Element::getDoubleWerte()
{
    Argument::DoubleWerteMap::iterator i;

    if ((i = a->dval.find(name)) != a->dval.end()) return i->second;
    else
    {
        a->msg.perror(Argument::NO_DOUBLEARG,
                "%s ist keinen Gleitkommazahlenargument", name);
        return DoubleWerte();
    }
}

Argument::LongWerte Argument::Element::getLongWerte()
{
    Argument::LongWerteMap::iterator i;

    if ((i = a->lval.find(name)) != a->lval.end()) return i->second;
    else
    {
        a->msg.perror(Argument::NO_LONGARG,
                "%s ist keinen ganzzahliges Argument", name);
        return LongWerte();
    }
}

Argument::Element::operator std::string()
{
    Argument::StringWerteMap::iterator i;

    if ((i = a->sval.find(name)) != a->sval.end())
    {
        if (i->second.size() != 1) a->msg.pwarning(Argument::MORE_ARGS,
                "%s hat nicht exakt einen Parameter", name);
        return i->second[0];
    }
    else
    {
        a->msg.perror(Argument::NO_STRINGARG,
                "%s ist keinen Zeichenkettenargument", name);
        return "";
    }
}

Argument::Element::operator double()
{
    Argument::DoubleWerteMap::iterator i;

    if ((i = a->dval.find(name)) != a->dval.end())
    {
        if (i->second.size() != 1) a->msg.pwarning(Argument::MORE_ARGS,
                "%s hat nicht exakt einen Parameter", name);
        return i->second[0];
    }
    else
    {
        a->msg.perror(Argument::NO_DOUBLEARG,
                "%s ist keinen Gleitkommazahlenargument", name);
        return 0.0;
    }
}

Argument::Element::operator long()
{
    Argument::LongWerteMap::iterator i;

    if ((i = a->lval.find(name)) != a->lval.end())
    {
        if (i->second.size() != 1) a->msg.pwarning(Argument::MORE_ARGS,
                "%s hat nicht exakt einen Parameter", name);
        return i->second[0];
    }
    else
    {
        a->msg.perror(Argument::NO_LONGARG,
                "%s ist keinen ganzzahliges Argument", name);
        return 0;
    }
}

Argument::Element::operator int()
{
    return (long) (*this);
}

Argument::Element::operator short()
{
    long l;

    l = (*this);
    if (l & 0xffff0000) a->msg.pwarning(Argument::ARGTO_LARGE,
            "Argument %s mit Wert %d passt nicht in ein Short", name, l);

    return (short) l;
}

Argument::Element::operator bool()
{
    return (long) (*this);
}

Argument::AliasMap Argument::alias;
Argument::AliasMap Argument::values;
Argument::StringWerteMap Argument::sval;
Argument::DoubleWerteMap Argument::dval;
Argument::LongWerteMap Argument::lval;
std::string Argument::name;
std::string Argument::fullname;

Argument::Argument(ListeMap *liste, std::string name) :
    msg("Argument")
{
    std::string::size_type i;

    if ((i = name.find_last_of('.')) != std::string::npos)
    {
        if (name.substr(i + 1) == "exe") this->fullname = name.substr(0, i);
        else this->fullname = name;
    }
    else
    {
        this->fullname = name;
    }

    if ((i = fullname.find_last_of("/\\")) != std::string::npos) this->name
            = fullname.substr(i + 1);
    else this->name = fullname;

    reset(liste);
}

Argument::Argument() :
    msg("Argument")
{
}

Argument::~Argument()
{
}

void Argument::reset(ListeMap *liste)
{

    ListeMap::iterator l, le;
    FILE *fp;
    char host[1024];
    unsigned int i;

    alias.clear();
    values.clear();
    sval.clear();
    dval.clear();
    lval.clear();

    for ( l = liste->begin(); l != liste->end(); ++l )
        l->second.defaults = l->second.orig_defaults;

    gethostname(host, sizeof(host));
    for ( i=0; i<sizeof(host); i++ )
      if ( host[i] == '.' || host[i] == '\0' ) break;
    host[i] = '\0';

    if ((fp = fopen((this->name + ".arg").c_str(), "r")) == NULL)
    {
        fp = fopen((fullname + ".arg").c_str(), "r");
    }

    if ( fp != NULL ){
        char buffer[10240];
        char *c;
        std::string str;
        std::string::size_type i;

        while ((c = fgets(buffer, sizeof(buffer), fp)) != NULL)
        {
            buffer[sizeof(buffer) - 1] = '\0';
            if (buffer[strlen(buffer) - 1] == '\n') buffer[strlen(buffer) - 1] = '\0';
            if (buffer[strlen(buffer) - 1] == '\r') buffer[strlen(buffer) - 1] = '\0';

            while (buffer[strlen(buffer) - 1] == '\\')
            {
                fgets(&buffer[strlen(buffer) - 1], sizeof(buffer) - strlen( buffer) + 1, fp);
                buffer[sizeof(buffer) - 1] = '\0';
                if (buffer[strlen(buffer) - 1] == '\n') buffer[strlen(buffer) - 1] = '\0';
                if (buffer[strlen(buffer) - 1] == '\r') buffer[strlen(buffer) - 1] = '\0';
            }

            while (isspace(*c))
                c++;

            // Kommentarzeile bzw. leere Zeilen überspringen
            if (*c == '#' || *c == '\0') continue;

            str = c;
            if ((i = str.find_first_of(':')) == std::string::npos)
            {
                msg.perror(WRONG_FORMAT, "Kann kein ':' in der Zeile der Konfigurationsdatei %s finden", (fullname + ".arg").c_str());
                msg.line("%s", buffer);
                continue;
            }
            else
            {
                std::string arg, par, h;

                arg = str.substr(0, i);
                par = str.substr(i + 1);
                if ((i = arg.find_first_of('@')) != std::string::npos)
                {
                    h = arg.substr(i + 1);
                    arg = arg.substr(0, i);
                }
                else
                {
                    h = "";
                }

                if (h == "" || h == host)
                {
                    if (par.find_first_not_of(" \t") != std::string::npos)
                        par = par.substr(par.find_first_not_of(" \t"));

                    if ((l = liste->find(arg)) == liste->end())
                        msg.perror( UNKOWN_ARGUMENT, "%s ist kein Parameter", arg.c_str());
                    else if (l->second.anzahl != 0) l->second.defaults = par;
                    else l->second.defaults = "1";
                }
            }
        }
        fclose(fp);
    }

    for (l = liste->begin(), le = liste->end(); l != le; ++l)
    {
        std::string name = l->first;
        std::string aname = l->second.alias;
        int anzahl = l->second.anzahl;
        char typ = l->second.typ;

        if (alias.find(l->second.alias) != alias.end())
        {
            msg.perror(DUP_ALIAS, "alias <%s> ist doppelt vorhanden - wird ignoriert", l->second.alias.c_str());
            continue;
        }

        alias[aname] = Alias(name, anzahl, typ);
        values[name] = Alias(aname, anzahl, typ);

        mkvalue(name, typ, l->second.defaults, anzahl);
    }

    umask( (long)this->operator[]("umask"));

}

void Argument::mkvalue(std::string name, char typ, std::string str, int oanzahl)
{
    std::string::size_type j;
    int i, anzahl;
    std::string val;

    anzahl = oanzahl;
    if (oanzahl < 0) anzahl = -oanzahl;
    if (oanzahl == 0) anzahl = 1;

    switch (typ)
    {
    case 'c':
        sval[name].clear();
        break;
    case 'd':
    case 'f':
        dval[name].clear();
        break;
    case 'i':
    case 'l':
        lval[name].clear();
        break;
    }

    for (i = 0; i < anzahl; i++)
    {
        if ((j = str.find_first_of(',')) == std::string::npos)
        {
            if (oanzahl > 0 && i != (anzahl - 1))
            {
                msg.perror(LESS_DEF, "Zu wenige default Werte für Argument %s vorhanden", name.c_str());
                exit(1);
            }
            else if ( oanzahl < 0 && str == "" )
                break;
            val = str;
            str = "";
        }
        else
        {
            val = str.substr(0, j);
            str = str.substr(j + 1);
        }

        switch (typ)
        {
        case 'c':
            sval[name].push_back(val);
            break;
        case 'd':
        case 'f':
        {
            double v = strtod(val.c_str(), NULL);
            dval[name].push_back(v);
            break;
        }
        case 'i':
        case 'l':
        {
            long v = strtol(val.c_str(), NULL, 0);
            lval[name].push_back(v);
            break;
        }
        }
    }
    if (str != "") msg.perror(MORE_ARGS, "Zu viele Werte in %s für Argument %s vorhanden", str.c_str(), name.c_str());
}

int Argument::scan(int &argc, char **&argv)
{
    AliasMap::iterator a;
    StringWerteMap::iterator si;
    DoubleWerteMap::iterator di;
    LongWerteMap::iterator li;
    std::string name;
    int typ, anzahl;
    int error_found;
    int i;

    error_found = 0;
    while (argc > 0)
    {

        if (**argv != '-') return error_found;

        if ((a = alias.find(*argv)) != alias.end())
        {
            name = a->second.name;
            typ = a->second.typ;
            anzahl = a->second.anzahl;
            if (anzahl < 0) anzahl = -anzahl;

            argc--;
            argv++;

            switch (typ)
            {
            case 'c':
                if ((si = sval.find(name)) == sval.end())
                {
                    msg.perror(NO_ALIAS, "Argument %s ist kein "
                        "Zeichenkettenargument", *argv);
                    error_found = -1;
                }

                si->second.clear();
                for (; anzahl > 0 && argc > 0; argc--, argv++, anzahl--)
                {
                    if ((*argv)[0] == '-' && (*argv)[1] == '-') break;
                    if (alias.find(*argv) != alias.end()) break;

                    si->second.push_back(*argv);
                }

                for (i = anzahl; i > 0; i--)
                    si->second.push_back("");

                break;

            case 'd':
            case 'f':
                if ((di = dval.find(name)) == dval.end())
                {
                    msg.perror(NO_ALIAS, "Argument %s ist kein "
                        "Gleitkommazahlenargument", *argv);
                    error_found = -1;
                }

                di->second.clear();
                for (; anzahl > 0 && argc > 0; argc--, argv++, anzahl--)
                {
                    double v;
                    if ((*argv)[0] == '-' && (*argv)[1] == '-') break;
                    if (alias.find(*argv) != alias.end()) break;

                    v = 0.0;
                    if (sscanf(*argv, "%lf", &v) == 0) msg.perror(NO_FLOAT,
                            "%s ist keine "
                                "Gleitkommazahl", *argv);
                    di->second.push_back(v);

                }

                for (i = anzahl; i > 0; i--)
                    di->second.push_back(0.0);

                break;
            case 'l':
            case 'i':
                if ((li = lval.find(name)) == lval.end())
                {
                    msg.perror(NO_ALIAS, "Argument %s ist kein "
                        "Ganzzahlenargument", *argv);
                    error_found = -1;
                }

                li->second.clear();

                if (anzahl == 0) li->second.push_back(1);

                for (; anzahl > 0 && argc > 0; argc--, argv++, anzahl--)
                {
                    long v;
                    if ((*argv)[0] == '-' && (*argv)[1] == '-') break;
                    if (alias.find(*argv) != alias.end()) break;

                    v = 0;
                    if (sscanf(*argv, "%li", &v) == 0) msg.perror(NO_INT,
                            "%s ist keine "
                                "Ganze Zahl", *argv);
                    sscanf(*argv, "%li", &v);
                    li->second.push_back(v);

                }

                for (i = anzahl; i > 0; i--)
                    li->second.push_back(0);

                break;
            }

            if (anzahl != 0 && a->second.anzahl > 0)
            {
                msg.perror(LESS_ARG, "%d Argumente für %s erwartet",
                        a->second.anzahl, a->first.c_str());
                error_found = -1;
            }

            if (argc > 0 && (*argv)[0] == '-' && (*argv)[1] == '-')
            {
                argv++;
                argc--;
            }
        }
        else
        {
            msg.perror(UNKOWN_ARGUMENT, "Argument %s ist unbekannt", *argv);
            argv++;
            argc--;
            error_found = -1;
        }
    }
    return error_found;
}

char Argument::exists(const char *name)
{
    if (sval.find(name) != sval.end()) return 'c';
    if (dval.find(name) != dval.end()) return 'd';
    if (lval.find(name) != lval.end()) return 'l';
    return '\0';
}

