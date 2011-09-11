#ifndef xml_parse_mne
#define xml_parse_mne


#include "string"
#include "map"
#include "vector"

#include <message/message.h>

class XmlParseNode;
class XmlParseNode
{
public:
    enum ERROR_TYPES
    {
        E_OK,
        E_NOENDTAG,
	E_TAGNOTCLOSE,
	E_TAGNOTOPEN,
	E_ARGUMENT,
	E_DELIMITER,
        E_MAXTYPE
    };

    typedef std::map<std::string, std::string> AttrMap;
private:
    Message *msg;
    XmlParseNode *parent;
    std::vector<XmlParseNode *> childs;
    std::string id;
    std::string data;
    AttrMap attrs;
    
    int parse_attr(std::string stag);

public:
    XmlParseNode(Message *msg, XmlParseNode *parent);
    ~XmlParseNode();

    int setXml(std::string stag, std::string content);

    XmlParseNode *getParent() { return parent; }
    std::string getId() { return id; }
    std::string getData() { return data; }
    AttrMap    *getAttrs() { return &attrs; }
    void        setAttr(const char* name, const char *value)
        { attrs[name] = value; }
    std::string getAttr( const char *name)
    {
        AttrMap::iterator i;
	if ( ( i = attrs.find(name) ) != attrs.end() ) return i->second;
	else return "";
    }


    XmlParseNode *getChild(unsigned int i)
	{ if ( i < childs.size() ) return childs[i]; else return NULL; }
    XmlParseNode *operator[] (unsigned int i) { return getChild(i); }
    unsigned int size() { return childs.size(); }

};


class XmlParse
{
protected:
    Message msg;
    XmlParseNode root;
    int error_found;

public:
    XmlParse();
    ~XmlParse();

    int setXml(std::string xml);

    XmlParseNode *getRoot() { return &root; }
};

#endif /* xml_parse_mne */
