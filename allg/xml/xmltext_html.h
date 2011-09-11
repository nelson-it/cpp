#ifndef xmltext_html_mne
#define xmltext_html_mne

#include <map>
#include <string>

#include "xmltext.h"

class XmlTextHtml : public XmlText
{
protected:
    enum ERROR_TYPES
    {
       E_PARSE = XmlText::E_MAXERROR
    };

    virtual void mk_start();
    virtual void mk_end();

    virtual void mk_text(XmlParseNode *node, int num);
    virtual void mk_part(XmlParseNode *node, int num);
    virtual void mk_itemize(XmlParseNode *node, int num);
    virtual void mk_enumerate(XmlParseNode *node, int num);
    virtual void mk_item(XmlParseNode *node, int num);
    virtual void mk_table(XmlParseNode *node, int num);
    virtual void mk_tabrow(XmlParseNode *node, int num);
    virtual void mk_tabcol(XmlParseNode *node, int num);

    virtual void mk_part_end(XmlParseNode *node, int num);
    virtual void mk_itemize_end(XmlParseNode *node, int num);
    virtual void mk_enumerate_end(XmlParseNode *node, int num);
    virtual void mk_item_end(XmlParseNode *node, int num);
    virtual void mk_table_end(XmlParseNode *node, int num);
    virtual void mk_tabrow_end(XmlParseNode *node, int num);
    virtual void mk_tabcol_end(XmlParseNode *node, int num);

    std::map<std::string,std::string> classmap;
    std::string html;
    int size;

    std::map<std::string,std::string> fonts;

public:
    XmlTextHtml(std::string size,
                std::map<std::string, std::string> classmap );
    virtual ~XmlTextHtml();
};

#endif /* xmltext_html_mne */
