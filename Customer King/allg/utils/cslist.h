#ifndef cs_list_mne
#define cs_list_mne

#include <vector>
#include <string>

class CsList
{
public:
    class Element
    {
        std::string value;
    public:
        Element(std::string value)
	{ this->value = value; }

	operator double();
	operator float();
	operator long();
	operator int();
	operator unsigned int();
	operator short();
	int operator ==(std::string);
    };

    typedef std::vector<std::string> Elements;
private:
    std::string cs_string;
    Elements list;
    int cs_string_valid;
    int ignore_lastempty;

    void mk_cs_string();

    Elements::iterator f;
public:
    CsList( int ignore_lastempty = 0 );
    CsList(std::string cs_string, char sep = ',' , int ignore_lastempty = 0);
    CsList(std::vector<std::string> *elements, int ignore_lastempty = 0);
    CsList(std::vector<std::string> elements, int ignore_lastempty = 0);

    ~CsList() {};

    std::string getString()
    {
        if ( ! cs_string_valid )
	    mk_cs_string();
	return cs_string;
    };

    CsList operator+ (const CsList &l1) const;
    operator std::string() { return cs_string; }

    void setString(std::string cs_string, char sep = ',' ,int ignore_lastempty = 0 );
    void add(std::string item)
    {
      cs_string_valid = 0;
      list.push_back(item);
    }


    std::string operator[] ( unsigned int i );

    std::vector<std::string>::iterator begin() { return list.begin(); }
    std::vector<std::string>::iterator end()   { return list.end(); }

    unsigned int empty() { return list.empty(); }
    unsigned int size() { return list.size(); }
    void reset() { f = list.end(); }
    std::string::size_type find( std::string str, int next = 0);

};

#endif /* cs_list_mne */

