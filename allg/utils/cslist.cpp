#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include "cslist.h"

CsList::Element::operator double()
{
    return strtod(value.c_str(), NULL);
}

CsList::Element::operator float()
        {
    return strtof(value.c_str(), NULL);
        }

CsList::Element::operator long()
        {
    return strtol(value.c_str(), NULL, 0);
        }

CsList::Element::operator int()
        {
    return (int)strtol(value.c_str(), NULL, 0);
        }

CsList::Element::operator unsigned int()
{
    return (unsigned int)strtol(value.c_str(), NULL, 0);
}

CsList::Element::operator short()
        {
    return (short)strtol(value.c_str(), NULL, 0);
        }

int CsList::Element::operator ==(std::string str)
        {
    return value == str;
        }

CsList::CsList(int ignore_lastempty, char sep )
{
    cs_string_valid = 1;
    this->ignore_lastempty = ignore_lastempty;
    this->sep = sep;
    f = list.end();
}

CsList::CsList( char sep )
{
    cs_string_valid = 1;
    this->ignore_lastempty = 0;
    this->sep = sep;
    f = list.end();
}

CsList::CsList(std::string cs_string, char sep, int ignore_lastempty)
{
    setString(cs_string, sep, ignore_lastempty);
}

CsList::CsList(std::vector<std::string> *elements, int ignore_lastempty)
{
    list = *elements;
    this->ignore_lastempty = ignore_lastempty;
    f = list.end();
    cs_string_valid = 0;
}

CsList::CsList(std::vector<std::string> elements, int ignore_lastempty)
{
    list = elements;
    this->ignore_lastempty = ignore_lastempty;
    f = list.end();
    cs_string_valid = 0;
}

void CsList::mk_cs_string(char sep)
{

    if ( list.empty() )
    {
        cs_string = "";
    }
    else
    {
        std::vector<std::string>::iterator i;
        i=list.begin();
        cs_string = *i++;

        for ( ; i != list.end(); ++i )
            cs_string += sep + *i;
    }
}

void CsList::setString(std::string cs_string, char sep, int ignore_lastempty)
{
    std::string::size_type i;

    this->sep = sep;
    this->cs_string = cs_string;
    this->ignore_lastempty = ignore_lastempty;
    cs_string_valid = 1;

    list.clear();
    i = 0;
    while( ( i = cs_string.find(sep,i)) != std::string::npos )
    {
        if ( i == 0 || cs_string[i-1] != '\\' )
        {
            list.push_back(cs_string.substr(0, i));
            cs_string = cs_string.substr(i+1);
            i = 0;
        }
        else
        {
            cs_string = cs_string.substr(0,i-1) + cs_string.substr(i);
            i++;
        }
    }

    if ( this->cs_string != "" && ( cs_string != "" || ignore_lastempty == 0 ) )
        list.push_back(cs_string);

    f = list.end();
}

CsList CsList::operator+( const CsList &l1 ) const
{
    CsList l2 = l1;
    if ( list.size() > 0 )
        return CsList( cs_string + this->sep + l2.getString(this->sep));
    else
        return l1;
}

std::string::size_type CsList::find(std::string str, int next )
{

    if ( next == 0 || f == list.end() ) this->f = list.begin(); else ++f;
    f = std::find(f, list.end(), str);
    if ( f == list.end() )
        return std::string::npos;
    else
        return ( f - list.begin() );
}

std::string CsList::operator [](unsigned int i)
{
    if ( i >= list.size() )
        return "";

    return list[i];
}

