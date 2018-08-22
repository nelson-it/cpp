#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include <utils/tostring.h>
#include "xmltext_tex.h"


XmlTextTex::XmlTextTex()
{
    partbegin = "{";
    partend = "}\\par";

    centerbegin = "{\\trivlist\\centering\\item\\vspace{-\\baselineskip}";
    centerend = "\\endtrivlist}\n";

    rightbegin = "{\\trivlist\\raggedleft\\item\\vspace{-\\baselineskip}";
    rightend = "\\endtrivlist}\n";

    leftbegin = "{\\trivlist\\raggedright\\item\\vspace{-\\baselineskip}";
    leftend = "\\endtrivlist}\n";
}

XmlTextTex::~XmlTextTex()
{
}

void XmlTextTex::mk_text(XmlParseNode *node, int num)
{
    std::string size;

    size = node->getAttr("size");

    if ( node->getData() == "" )
    {
        if ( emptytext == -1 ) emptytext = 1;
        return;
    }


    emptytext = 0;

    add_content("{");

    if ( size == "xxs" )
        add_content("\\tiny ");
    else if ( size == "xs" )
        add_content("\\footnotesize ");
    else if ( size == "s" )
        add_content("\\small ");
    else if ( size == "n" )
        add_content("\\normalsize ");
    else if ( size == "l" )
        add_content("\\large ");
    else if ( size == "xl" )
        add_content("\\Large ");
    else if ( size == "xxl" )
        add_content("\\LARGE ");

    if ( node->getAttr("weight") == "bold" )
        add_content("\\bf ");

    if ( node->getAttr("style") == "italic" )
        add_content("\\it ");

    if ( node->getData() == " " )
        add_content("\\ ");
    else
        add_content("%s", ToString::mktex(node->getData()).c_str());
}

void XmlTextTex::mk_text_end(XmlParseNode *node, int num)
{
    if ( node->getData() == "" )
        return;

    add_content("}");
}


void XmlTextTex::mk_part(XmlParseNode *node, int num)
{
    add_content("%s", partbegin.c_str());

    if ( node->getAttr("align") == "left" )
    	add_content("%s", leftbegin.c_str());
    if ( node->getAttr("align") == "center" )
    	add_content("%s", centerbegin.c_str());
    if ( node->getAttr("align") == "right" )
    	add_content("%s", rightbegin.c_str());

    emptytext = -1;
}

void XmlTextTex::mk_part_end(XmlParseNode *node, int num)
{
	   if ( node->getAttr("align") == "left" )
	    	add_content("%s", leftend.c_str());
	   if ( node->getAttr("align") == "center" )
	    	add_content("%s", centerend.c_str());
	   if ( node->getAttr("align") == "right" )
	    	add_content("%s", rightend.c_str());

       if ( emptytext == 1 )
       {
           add_content("}");
           emptytext = -1;
       }
       else
	    add_content("%s", partend.c_str());
}


void XmlTextTex::mk_itemize(XmlParseNode *node, int num)
{
    node->setAttr("partbegin",partbegin.c_str());
    node->setAttr("partend",partend.c_str());

    partbegin = "{";
    partend = "}\\par";

    if ( node->getAttr("relwidth") != "" )
         add_content("\\parbox{0.%s\\textwidth}{",node->getAttr("relwidth").c_str());
    add_content("\\begin{itemize}");
}

void XmlTextTex::mk_itemize_end(XmlParseNode *node, int num)
{
    add_content("\\end{itemize}%%\n");

    if ( node->getAttr("relwidth") != "" )
        add_content("}%%\n");

    partbegin = node->getAttr("partbegin");
    partend = node->getAttr("partend");
}


void XmlTextTex::mk_enumerate(XmlParseNode *node, int num)
{
    node->setAttr("partbegin",partbegin.c_str());
    node->setAttr("partend",partend.c_str());

    partbegin = "{";
    partend = "}\\par";

    if ( node->getAttr("relwidth") != "" )
         add_content("\\parbox{0.%s\\textwidth}{",node->getAttr("relwidth").c_str());
    add_content("\\begin{enumerate}");
}

void XmlTextTex::mk_enumerate_end(XmlParseNode *node, int num)
{
    add_content("\\end{enumerate}%%\n");

    if ( node->getAttr("relwidth") != "" )
        add_content("}%%\n");
    partbegin = node->getAttr("partbegin");
    partend = node->getAttr("partend");

}


void XmlTextTex::mk_item(XmlParseNode *node, int num)
{
    add_content("\\item");
}

void XmlTextTex::mk_item_end(XmlParseNode *node, int num)
{
}


void XmlTextTex::mk_table(XmlParseNode *node, int num)
{
    unsigned int i,max_cols, border;
    const char *padding = "@{}";

    max_cols = strtoul(node->getAttr("colcount").c_str(), NULL, 10 );
    border = node->getAttr("border") == "1";

    if ( node->getAttr("padding") != "0" ) padding = "";

    if ( node->getAttr("align") == "center" )
	    add_content("{\\trivlist\\centering\\item");

	if ( node->getAttr("align")== "right" )
	    add_content("{\\trivlist\\raggedleft\\item");

    add_content("\\begin{tabular}{");

    for ( i=0; i<max_cols; ++i )
    {
    	char format[256];
    	char str[256];
    	if ( border )
            add_content("|");

    	sprintf(str, "width%d", i);
    	if ( node->getAttr(str) != "" )
    		sprintf(format, "p{0.%s\\textwidth}", node->getAttr(str).c_str());
    	else
    		sprintf(format, "l");

    	add_content("%s%s%s", padding, format, padding);
    }

    if ( border )
        add_content("|");
    add_content("}\n" );

    if ( border )
        add_content("\\hline");

}

void XmlTextTex::mk_table_end(XmlParseNode *node, int num)
{
    add_content("\\end{tabular}");

    if ( node->getAttr("align") == "center" || node->getAttr("align") == "left")
	    add_content("\\endtrivlist}\n");
    else
        add_content("\\par\n");
}

void XmlTextTex::mk_tabrow(XmlParseNode *node, int num)
{
    act_tab->setAttr("colsep", "");
}

void XmlTextTex::mk_tabrow_end(XmlParseNode *node, int num)
{
    if ( act_tab->getAttr("border") == "1" )
        add_content("\\\\\n\\hline\n");
    else
        add_content("\\\\\n");
}


void XmlTextTex::mk_tabcol(XmlParseNode *node, int num)
{
    node->setAttr("partbegin",partbegin.c_str());
    node->setAttr("partend",partend.c_str());

    node->setAttr("centerbegin",centerbegin.c_str());
    node->setAttr("centerend",centerend.c_str());

    node->setAttr("rightbegin",rightbegin.c_str());
    node->setAttr("rightend",rightend.c_str());

    centerbegin = "\\centering";
    centerend = "\\\\";

    rightbegin = "\\hfill";
    rightend = "";

    add_content("%s", act_tab->getAttr("colsep").c_str());
    act_tab->setAttr("colsep", "&");

}

void XmlTextTex::mk_tabcol_end(XmlParseNode *node, int num)
{
	partbegin = node->getAttr("partbegin");
    partend = node->getAttr("partend");

    centerbegin = node->getAttr("centerbegin");
    centerend = node->getAttr("centerend");

    rightbegin = node->getAttr("rightbegin");
    rightend = node->getAttr("rightend");
}

