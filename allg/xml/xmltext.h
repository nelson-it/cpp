#ifndef xml_text_mne
#define xml_text_mne

#include <stdio.h>

#include "xmlparse.h"

class XmlText : public XmlParse
{
public:
	enum OUTPUT_TYP
	{
		O_PLAIN = 0,
		O_XML = 1
	};

private:

	int act_rowcount;
	int act_colcount;

protected:

	XmlParseNode *act_tab;

	enum ERROR_TYPES
	{
		E_OK,
		E_PARSE,
		E_UNKNOWNID,
		E_MAXERROR = 100
	};

	enum ACT_NODE_TYP
	{
		N_TEXT,
		N_PART,
		N_ITEMIZE,
		N_ENUMERATE,
		N_ITEM,
		N_TABLE,
		N_TABROW,
		N_TABCOL
	};
	int act_node_typ;

	char *xmlstring;
	int xmllength;
	static const int XMLSIZE = 10240;

	OUTPUT_TYP typ;


	virtual void write(XmlParseNode *node, int num);
	virtual void compute(XmlParseNode *node);

	virtual void mk_start() {};
	virtual void mk_end() {};

	virtual void mk_text(XmlParseNode *node, int num) = 0;
	virtual void mk_text_end(XmlParseNode *node, int num) {};

	virtual void mk_part(XmlParseNode *node, int num) = 0;
	virtual void mk_part_end(XmlParseNode *node, int num) {};

	virtual void mk_itemize(XmlParseNode *node, int num) = 0;
	virtual void mk_itemize_end(XmlParseNode *node, int num) {};

	virtual void mk_enumerate(XmlParseNode *node, int num) = 0;
	virtual void mk_enumerate_end(XmlParseNode *node, int num) {};

	virtual void mk_item(XmlParseNode *node, int num) = 0;
	virtual void mk_item_end(XmlParseNode *node, int num) {};

	virtual void mk_table(XmlParseNode *node, int num) = 0;
	virtual void mk_table_end(XmlParseNode *node, int num) {};

	virtual void mk_tabrow(XmlParseNode *node, int num) = 0;
	virtual void mk_tabrow_end(XmlParseNode *node, int num) {};

	virtual void mk_tabcol(XmlParseNode *node, int num) = 0;
	virtual void mk_tabcol_end(XmlParseNode *node, int num) {};

	virtual void add_content( const char* format, ...);

public:
	XmlText() {act_tab = NULL; xmlstring = new char[XMLSIZE]; *xmlstring = '\0'; xmllength = XMLSIZE; };
	virtual ~XmlText(){ delete [] xmlstring; };

	void mk_output(OUTPUT_TYP typ = O_PLAIN );
	const char *get_string() { return xmlstring; }
};

#endif /* xml_text_mne */
