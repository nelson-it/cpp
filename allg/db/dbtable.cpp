#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(__CYGWIN__) || defined(__MINGW32__)
#include <time.h>
#endif


#include "dbtable.h"

DbTable::ColumnMapAll DbTable::all_cols;
std::map<std::string,char> DbTable::all_typ;

DbTable::Value::Value()
{
    buffer = NULL;
    size = 0;
    value = "";
}

DbTable::Value::Value( const Value &in )
{
    value = in.value;
    size = in.size;
    if ( size != 0 )
    {
        buffer = new char[size];
	memcpy(buffer, in.buffer, size);
    }
    else
    {
        buffer = NULL;
    }
}

DbTable::Value &DbTable::Value::operator=( const Value &in )
{
    value = in.value;
    size = in.size;
    if ( size != 0 )
    {
        buffer = new char[size];
	memcpy(buffer, in.buffer, size);
    }
    else
    {
        buffer = NULL;
    }

    return *this;
}

DbTable::Value::Value(const char *v)
{
    buffer = NULL;
    size = 0;
    value = v;
}

DbTable::Value::Value(char *v)
{
    buffer = NULL;
    size = 0;
    value = v;
}

DbTable::Value::Value(char *v, int size)
{
    buffer = new char[size];
    memcpy(buffer, v, size);

    value = "";
    this->size = size;
}

DbTable::Value::Value(std::string v)
{
    buffer = NULL;
    value = v;
    size = 0;
}

DbTable::Value::Value(int v)
{
    buffer = NULL;
    size = 0;
    char str[32];
    sprintf(str,"%20d", v);
    value = str;
}

DbTable::Value::Value(long v)
{
    buffer = NULL;
    size = 0;
    char str[32];
    sprintf(str,"%20ld", v);
    value = str;
}

DbTable::Value::Value(double v)
{
    buffer = NULL;
    size = 0;
    char str[1024];
    sprintf(str,"%40.20f", v);
    value = str;
}

DbTable::Value::~Value()
{
    if ( buffer != NULL )
        delete buffer;
}

DbTable::Column::Column()
{
    this->typ = -1;
    this->size = 0;
    this->can_null = 0;
    this->dpytyp = -1;
    int_typ = 0;

}
DbTable::Column::Column(std::string name)
{
    this->name = name;
    this->typ = -1;
    this->size = 0;
    this->can_null = 0;
    this->dpytyp = -1;
    int_typ = 0;

}

DbTable::Column::Column(std::string name,int typ, std::string value, int can_null, std::string regexp, long dpytyp)
{
    this->name = name;
    this->typ = typ;
    this->size = 0;
    this->can_null = can_null;
    this->value = value;
    this->regexp = regexp;
    this->dpytyp = dpytyp;

    int_typ = 0;
}

DbTable::Column::Column(std::string name, int typ, int size,
                        std::string value, int can_null, std::string regexp, long dpytyp)
{
    this->name = name;
    this->typ = typ;
    this->size = size;
    this->can_null = can_null;
    this->value = value;
    this->regexp = regexp;
    this->dpytyp = dpytyp;

    int_typ = 0;
}

std::string DbTable::Column::styp(int t)
{
    if ( t < 0 ) t = this->typ;

    switch(t)
    {
    case DbConnect::UNKNOWN:
	return "";
	break;
    case DbConnect::BOOL:
	return "bool";
	break;
    case DbConnect::CHAR:
	return "char";
	break;
    case DbConnect::SHORT:
	return "short";
	break;
    case DbConnect::LONG:
	return "long";
	break;
    case DbConnect::FLOAT:
	return "float";
	break;
    case DbConnect::DOUBLE:
	return "double";
	break;
    default:
	{
	char str[32];
	sprintf(str, "Typ%d", t );
	return str;
	}
    }
}

void DbTable::start_columncheck()
{
    ColumnMap::iterator i;

    for( i = cols.begin(); i != cols.end(); ++i )
	i->second.have_value = i->second.can_null || i->second.value != "";
}

int DbTable::columncheck()
{
    ColumnMap::iterator i;
    int result = 0;

    for( i = cols.begin(); i != cols.end(); ++i )
    {
	if ( ! i->second.have_value )
	{
	   msg.perror(NO_VALUES, "Spalte %s benötigt einen Wert",
	                          (this->schema + ":" + this->name + ":" + i->first).c_str());
	   result = -1;
	}
    }
    return result;
}

std::string DbTable::getValue( const Column *col )
{
    if ( ( col->can_null & Column::DBNULL ) && col->value == "" ) return "null";
    return dbconnect->getValue(col->typ, col->value);
}

int DbTable::mk_usertime_columns()
{

    int result = 0;
    char now[64];

    #define ND  DbTable::Column::NOTNULL_NOTDEF

    sprintf(now,"%ld", time(NULL));

    if ( cols.find("createdate") == cols.end() )
	result  = add_column("createdate",
	                     Column("createdate",DbConnect::LONG, "0", ND));
    if ( cols.find("createuser") == cols.end() )
	result |= add_column("createuser",
	                      Column("createuser",DbConnect::CHAR,32,"",ND));
    if ( cols.find("modifydate") == cols.end() )
	result |= add_column("modifydate",
	                      Column("modifydate",DbConnect::LONG, now, ND));
    if ( cols.find("modifyuser") == cols.end() )
	result |= add_column("modifyuser",
	                      Column("modifyuser",DbConnect::CHAR,32,"",ND));

    return result;
}


DbTable::Column DbTable::getColumn(std::string name)
{
    ColumnMap::iterator i;
    if ( ( i = cols.find(name)) != cols.end() )
        return i->second;
    else
        return Column();
}
