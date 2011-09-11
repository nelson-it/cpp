#ifndef xmltext_htmledit_mne
#define xmltext_htmledit_mne

#include <string>

#include "xmltext.h"

class XmlTextHtmlEdit : public XmlText
{
protected:
    enum ERROR_TYPES
    {
       E_PARSE = XmlText::E_MAXERROR
    };

    virtual void mk_start();
    virtual void mk_text(XmlParseNode *node, int num);
    virtual void mk_part(XmlParseNode *node, int num);
    virtual void mk_itemize(XmlParseNode *node, int num);
    virtual void mk_enumerate(XmlParseNode *node, int num);
    virtual void mk_item(XmlParseNode *node, int num);
    virtual void mk_table(XmlParseNode *node, int num);
    virtual void mk_tabrow(XmlParseNode *node, int num);
    virtual void mk_tabcol(XmlParseNode *node, int num);

    virtual void mk_itemize_end(XmlParseNode *node, int num);
    virtual void mk_enumerate_end(XmlParseNode *node, int num);
    virtual void mk_table_end(XmlParseNode *node, int num);
    std::string editor;

public:
    XmlTextHtmlEdit(std::string editor);
    virtual ~XmlTextHtmlEdit();
};

#endif /* xmltext_htmledit_mne */
