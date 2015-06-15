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
    char sep;

    void mk_cs_string(char sep);

    Elements::iterator f;
public:
    CsList( int ignore_lastempty = 0, char sep = ',' );
    CsList( char sep );
    CsList(std::string cs_string, char sep = ',' , int ignore_lastempty = 0);
    CsList(std::vector<std::string> *elements, int ignore_lastempty = 0);
    CsList(std::vector<std::string> elements, int ignore_lastempty = 0);

    ~CsList() {};

    std::string getString()
    {
        if ( ! cs_string_valid )
            mk_cs_string(this->sep);
        return cs_string;
    };

    std::string getString(char sep)
    {
        mk_cs_string(sep);
        if ( sep != this->sep ) cs_string_valid = 0;
        return cs_string;
    };

    CsList operator+ (const CsList &l1) const;
    operator std::string() { return getString(); }

    void setString(std::string cs_string, char sep = ',' ,int ignore_lastempty = 0 );
    void add(std::string item)
    {
      cs_string_valid = 0;
      list.push_back(item);
    }


    std::string operator[] ( unsigned int i );

    std::vector<std::string>::iterator begin() { return list.begin(); cs_string_valid = 0; }
    std::vector<std::string>::iterator end()   { return list.end(); }

    unsigned int empty() { return list.empty(); }
    unsigned int size() { return list.size(); }
    void clear() { list.clear(); cs_string_valid = 0; }

    void reset() { f = list.end(); }
    std::string::size_type find( std::string str, int next = 0);

};

#endif /* cs_list_mne */

