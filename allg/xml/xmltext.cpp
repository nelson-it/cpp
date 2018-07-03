#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "xmltext.h"

char* itoa(int value, char* result, int base)
{
	// check that the base if valid
	if (base < 2 || base > 16)
	{
		*result = 0;
		return result;
	}

	char* out = result;
	int quotient = value;
	do
	{
		*out = "0123456789abcdef"[abs(quotient % base)];
		++out;
		quotient /= base;
	} while (quotient);

	// Only apply negative sign for base 10

	if (value < 0 && base == 10)
		*out++ = '-';

	std::reverse(result, out);
	*out = 0;
	return result;
}

void XmlText::compute(XmlParseNode *node)
{
	std::string id;
	unsigned int i;
	char buffer[128];


	id = node->getId();

	if ( id == "table" )
	{
		unsigned int save_row;
		unsigned int save_col;
		XmlParseNode *save_tab;

		save_row = act_rowcount;
		save_col = act_colcount;
		save_tab = act_tab;

		act_rowcount = 0;
		act_colcount = 0;

		act_tab = node;

		if ( act_tab->getAttr("border") == "" )
			act_tab->setAttr("border", "0");

		if ( act_tab->getAttr("padding") == "" )
			act_tab->setAttr("padding", "0");

		for ( i=0; i< node->size(); ++i ) compute(node->getChild(i));
		act_tab->setAttr("rowcount", itoa(act_rowcount, buffer, 10));
		act_tab->setAttr("colcount", itoa(act_colcount, buffer, 10));

		if ( save_tab != NULL )
			act_tab->setAttr("parentborder",
					save_tab->getAttr("border").c_str());
		else
			act_tab->setAttr("parentborder", "0" );

		act_rowcount = save_row;
		act_colcount = save_col;
		act_tab = save_tab;
	}
	else if ( id == "tr" )
	{
		act_colcount = 0;
		node->setAttr("rownum", itoa(act_rowcount, buffer, 10 ));
		for ( i=0; i< node->size(); ++i ) compute(node->getChild(i));
		act_rowcount++;
	}
	else if ( id == "td" )
	{
	    node->setAttr("colnum", itoa(act_colcount, buffer, 10 ));
	    for ( i=0; i< node->size(); ++i ) compute(node->getChild(i));
	    act_colcount++;

	    char str[256];
	    if ( node->getAttr("relwidth") != "")
	    {
	        sprintf(str, "width%d", act_colcount - 1 );
	        act_tab->setAttr(str, node->getAttr("relwidth").c_str());
	    }

	    if ( node->getAttr("relheight") != "")
	    {
	        sprintf(str, "height%d", act_colcount - 1 );
	        act_tab->setAttr(str, node->getAttr("relheight").c_str());
	    }

	    if ( node->getAttr("align") != "")
	    {
	        sprintf(str, "align%d", act_colcount - 1 );
	        act_tab->setAttr(str, node->getAttr("align").c_str());
	    }

	    if ( node->getAttr("valign") != "")
	    {
	        sprintf(str, "valign%d", act_colcount - 1 );
	        act_tab->setAttr(str, node->getAttr("valign").c_str());
	    }
	}
	else
	{
		for ( i=0; i< node->size(); ++i ) compute(node->getChild(i));
	}
}

void XmlText::write(XmlParseNode *node, int num)
{
	std::string id;
	unsigned int i;

	id = node->getId();

	if ( id == "text" )
	{
		mk_text(node, num);
		mk_text_end(node, num);
	}
	else if ( id == "part" )
	{
		mk_part(node,num);
		for ( i=0; i< node->size(); ++i ) write(node->getChild(i),i);
		mk_part_end(node,num);
	}
	else if ( id == "itemize" )
	{
		mk_itemize(node,num);
		for ( i=0; i< node->size(); ++i ) write(node->getChild(i),i);
		mk_itemize_end(node,num);
	}
	else if ( id == "enumerate" )
	{
		mk_enumerate(node,num);
		for ( i=0; i< node->size(); ++i ) write(node->getChild(i),i);
		mk_enumerate_end(node,num);
	}
	else if ( id == "item" )
	{
		mk_item(node,num);
		for ( i=0; i< node->size(); ++i ) write(node->getChild(i),i);
		mk_item_end(node,num);
	}
	else if ( id == "table" )
	{
		XmlParseNode *save_tab;

		save_tab = act_tab;
		act_tab = node;

		mk_table(node,num);
		for ( i=0; i< node->size(); ++i ) write(node->getChild(i),i);
		mk_table_end(node,num);

		act_tab = save_tab;
	}
	else if ( id == "tr" )
	{
		mk_tabrow(node,num);
		for ( i=0; i< node->size(); ++i ) write(node->getChild(i),i);
		mk_tabrow_end(node,num);
	}
	else if ( id == "td" )
	{
		mk_tabcol(node,num);
		for ( i=0; i< node->size(); ++i ) write(node->getChild(i),i);
		mk_tabcol_end(node,num);
	}
	else
	{
		msg.perror(E_UNKNOWNID, "Tag <%s> ist unbekannt", id.c_str());
		return;
	}
}

void XmlText::add_content( const char *format, ... )
{
    int n;
    int oldsize = strlen(xmlstring);
    va_list ap;
    va_start(ap, format);

    n = vsnprintf(&xmlstring[strlen(xmlstring)], xmllength, format, ap);
    if ( ( oldsize + n + 1 ) >= xmllength )
    {
        while ( ( oldsize + n + 1 ) >= xmllength ) xmllength += XMLSIZE;
        char *str = new char[xmllength];
        memcpy(str, xmlstring, oldsize);
        str[oldsize] = '\0';
        delete[] xmlstring;
        xmlstring = str;

        va_start(ap, format);
        vsnprintf(&xmlstring[strlen(xmlstring)], xmllength, format, ap);
    }

}

void XmlText::mk_output(OUTPUT_TYP typ )
{

	unsigned int i;

	if ( error_found )
	{
		msg.perror(E_PARSE, "Ausgabe konnte wegen Fehlern im XML nicht erzeugt werden");
		return;
	}

	this->typ = typ;

	act_rowcount = 0;
	act_colcount = 0;
	for ( i=0; i<root.size(); ++i ) compute(root[i]);

	mk_start();
	for ( i=0; i<root.size(); ++i ) write(root[i], 0);
	mk_end();
}
