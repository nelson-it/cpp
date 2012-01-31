#ifndef xmltext_tex_mne
#define xmltext_tex_mne

#include <string>

#include "xmltext.h"

class XmlTextTex : public XmlText
{
    std::string partbegin;
    std::string partend;

    std::string centerbegin;
    std::string centerend;

    std::string rightbegin;
    std::string rightend;

protected:
    enum ERROR_TYPES
    {
       E_PARSE = XmlText::E_MAXERROR
    };

    virtual void mk_text(XmlParseNode *node, int num);
    virtual void mk_text_end(XmlParseNode *node, int num);

    virtual void mk_part(XmlParseNode *node, int num);
    virtual void mk_part_end(XmlParseNode *node, int num);

    virtual void mk_itemize(XmlParseNode *node, int num);
    virtual void mk_itemize_end(XmlParseNode *node, int num);

    virtual void mk_enumerate(XmlParseNode *node, int num);
    virtual void mk_enumerate_end(XmlParseNode *node, int num);
    
    virtual void mk_item(XmlParseNode *node, int num);
    virtual void mk_item_end(XmlParseNode *node, int num);
    
    virtual void mk_table(XmlParseNode *node, int num);
    virtual void mk_table_end(XmlParseNode *node, int num);

    virtual void mk_tabrow(XmlParseNode *node, int num);
    virtual void mk_tabrow_end(XmlParseNode *node, int num);

    virtual void mk_tabcol(XmlParseNode *node, int num);
    virtual void mk_tabcol_end(XmlParseNode *node, int num);

public:
    XmlTextTex();
    virtual ~XmlTextTex();
};

#endif /* xmltext_tex_mne */
