#ifndef argument_mne
#define argument_mne

#include <string>
#include <map>
#include <vector>

#include <string.h>

#include <message/message.h>

class Argument
{
protected:
    class Alias
    {
    public:
        std::string name;
	int anzahl;
	char typ;

	Alias () {};
	Alias ( std::string name, int anzahl, char typ )
	    { this->name = name, this->anzahl = anzahl; this->typ = typ; }

    };

public:
    class Liste
    {
	friend class Argument;

	std::string alias;
	char typ; // c = strings, d,f=double, i,l=long
	int anzahl;
	std::string defaults;
	std::string orig_defaults;
    public:
        Liste() {};
	Liste(std::string a )
	{
	    alias = a; typ = 'l'; anzahl = 0;
	}
	Liste(std::string a, char t, int n, std::string d )
	{
	    alias = a; typ = t; anzahl = n; defaults = orig_defaults = d;
	}
    };

    typedef std::map<std::string, Liste> ListeMap;

    typedef std::vector<std::string> StringWerte;
    typedef std::vector<double>      DoubleWerte;
    typedef std::vector<long>        LongWerte;

    typedef std::map<std::string, Alias> AliasMap;
    typedef std::map<std::string,StringWerte>  StringWerteMap;
    typedef std::map<std::string,DoubleWerte>  DoubleWerteMap;
    typedef std::map<std::string,LongWerte>    LongWerteMap;

protected:

    class Element
    {
	const char *name;
        Argument *a;

    public:
        Element(const char *name, Argument *a);

	operator std::string();
	operator double();
	operator float();
	operator long();
	operator int();
	operator short();
	operator bool();

	Argument::StringWerte getStringWerte();
	Argument::DoubleWerte getDoubleWerte();
	Argument::LongWerte   getLongWerte();
    };

    static std::string fullname;
    static std::string name;
    static AliasMap alias;
    static AliasMap values;

    static StringWerteMap sval;
    static DoubleWerteMap dval;
    static LongWerteMap   lval;

    enum ERROR_TYPES
    {
        OK = 0,
	LESS_DEF,
	NO_STRINGARG,
	NO_DOUBLEARG,
	NO_LONGARG,
	MORE_ARGS,
	ARGTO_LARGE,
	UNKOWN_ARGUMENT,
	NO_ALIAS,
        LESS_ARG,
	NO_FLOAT,
	NO_INT,
	WRONG_FORMAT,
	DUP_ALIAS,
	SAVE_WRONG_COUNT,
	SAVE_NOCOPY,
	SAVE_NOOPEN
    };

    Message msg;
    friend class Element;

    void mkvalue( std::string name, char typ, std::string value, int anzahl );
    void strip(char *buffer, unsigned int size)
    {
	buffer[size - 1] = '\0';
	while ( buffer[strlen(buffer) - 1] == '\n'||
		buffer[strlen(buffer) - 1] == '\r' )
	buffer[strlen(buffer) - 1] = '\0';
    }

public:
    Argument( ListeMap *liste, std::string name);
    Argument();
    ~Argument();

    void reset(ListeMap *liste);
    int scan(int &argc, char **&argv);

    std::string getName()  { return name; }
    std::string get_name() { return name; }

    std::string getFullname()  { return fullname; }
    std::string get_fullname() { return fullname; }

    Element operator[] ( const char *name);
    char exists(const char *name);

    };

#endif /* argument_mne */
