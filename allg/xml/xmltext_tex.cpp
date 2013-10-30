#ifdef PTHREAD
#include <pthread.h>
#endif

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

    fprintf(fp, "{");

    if ( size == "xxs" )
        fprintf(fp, "\\tiny ");
    else if ( size == "xs" )
        fprintf(fp, "\\footnotesize ");
    else if ( size == "s" )
        fprintf(fp, "\\small ");
    else if ( size == "n" )
        fprintf(fp, "\\normalsize ");
    else if ( size == "l" )
        fprintf(fp, "\\large ");
    else if ( size == "xl" )
        fprintf(fp, "\\Large ");
    else if ( size == "xxl" )
        fprintf(fp, "\\LARGE ");

    if ( node->getAttr("weight") == "bold" )
        fprintf(fp, "\\bf ");

    if ( node->getAttr("style") == "italic" )
        fprintf(fp, "\\it ");

    if ( node->getData() == " " )
        fprintf(fp, "\\ ");
    else
        fprintf(fp, "%s", ToString::mktex(node->getData()).c_str());
}

void XmlTextTex::mk_text_end(XmlParseNode *node, int num)
{
    if ( node->getData() == "" )
        return;

    fprintf(fp, "}");
}


void XmlTextTex::mk_part(XmlParseNode *node, int num)
{
    fprintf(fp, "%s", partbegin.c_str());

    if ( node->getAttr("align") == "left" )
    	fprintf(fp, "%s", leftbegin.c_str());
    if ( node->getAttr("align") == "center" )
    	fprintf(fp, "%s", centerbegin.c_str());
    if ( node->getAttr("align") == "right" )
    	fprintf(fp, "%s", rightbegin.c_str());

    emptytext = -1;
}

void XmlTextTex::mk_part_end(XmlParseNode *node, int num)
{
	   if ( node->getAttr("align") == "left" )
	    	fprintf(fp, "%s", leftend.c_str());
	   if ( node->getAttr("align") == "center" )
	    	fprintf(fp, "%s", centerend.c_str());
	   if ( node->getAttr("align") == "right" )
	    	fprintf(fp, "%s", rightend.c_str());

       if ( emptytext == 1 )
       {
           fprintf(fp, "}");
           emptytext = -1;
       }
       else
	    fprintf(fp, "%s", partend.c_str());
}


void XmlTextTex::mk_itemize(XmlParseNode *node, int num)
{
    node->setAttr("partbegin",partbegin.c_str());
    node->setAttr("partend",partend.c_str());

    partbegin = "{";
    partend = "}\\par";

    if ( node->getAttr("relwidth") != "" )
         fprintf(fp, "\\parbox{0.%s\\textwidth}{",node->getAttr("relwidth").c_str());
    fprintf(fp, "\\begin{itemize}");
}

void XmlTextTex::mk_itemize_end(XmlParseNode *node, int num)
{
    fprintf(fp, "\\end{itemize}%%\n");

    if ( node->getAttr("relwidth") != "" )
        fprintf(fp, "}%%\n");

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
         fprintf(fp, "\\parbox{0.%s\\textwidth}{",node->getAttr("relwidth").c_str());
    fprintf(fp, "\\begin{enumerate}");
}

void XmlTextTex::mk_enumerate_end(XmlParseNode *node, int num)
{
    fprintf(fp, "\\end{enumerate}%%\n");

    if ( node->getAttr("relwidth") != "" )
        fprintf(fp, "}%%\n");
    partbegin = node->getAttr("partbegin");
    partend = node->getAttr("partend");

}


void XmlTextTex::mk_item(XmlParseNode *node, int num)
{
    fprintf(fp, "\\item");
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
	    fprintf(fp, "{\\trivlist\\centering\\item");

	if ( node->getAttr("align")== "right" )
	    fprintf(fp, "{\\trivlist\\raggedleft\\item");

    fprintf(fp, "\\begin{tabular}{");

    for ( i=0; i<max_cols; ++i )
    {
    	char format[256];
    	char str[256];
    	if ( border )
            fprintf(fp, "|");

    	sprintf(str, "width%d", i);
    	if ( node->getAttr(str) != "" )
    		sprintf(format, "p{0.%s\\textwidth}", node->getAttr(str).c_str());
    	else
    		sprintf(format, "l");

    	fprintf(fp, "%s%s%s", padding, format, padding);
    }

    if ( border )
        fprintf(fp, "|");
    fprintf(fp, "}\n" );

    if ( border )
        fprintf(fp, "\\hline");

}

void XmlTextTex::mk_table_end(XmlParseNode *node, int num)
{
    fprintf(fp, "\\end{tabular}");

    if ( node->getAttr("align") == "center" || node->getAttr("align") == "left")
	    fprintf(fp, "\\endtrivlist}\n");
    else
        fprintf(fp, "\\par\n");
}

void XmlTextTex::mk_tabrow(XmlParseNode *node, int num)
{
    act_tab->setAttr("colsep", "");
}

void XmlTextTex::mk_tabrow_end(XmlParseNode *node, int num)
{
    if ( act_tab->getAttr("border") == "1" )
        fprintf(fp, "\\\\\n\\hline\n");
    else
        fprintf(fp, "\\\\\n");
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

    fprintf(fp, "%s", act_tab->getAttr("colsep").c_str());
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

