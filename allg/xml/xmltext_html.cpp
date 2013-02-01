#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include <utils/tostring.h>
#include "xmltext_html.h"

XmlTextHtml::XmlTextHtml(std::string size,
		std::map<std::string,std::string> classmap)
{

	if (size == "")
		size = "500";

	sscanf(size.c_str(), "%d", &this->size);

	if (classmap.find("mne_font") == classmap.end() || classmap["mne_font"]
			== "")
		classmap["mne_font"] = "mne_font";

	if (classmap.find("mne_bold") == classmap.end() || classmap["mne_bold"]
			== "")
		classmap["mne_bold"] = "mne_bold";

	if (classmap.find("mne_italic") == classmap.end() || classmap["mne_italic"]
			== "")
		classmap["mne_italic"] = "mne_italic";

	if (classmap.find("mne_alignc") == classmap.end() || classmap["mne_alignc"]
			== "")
		classmap["mne_alignc"] = "mne_alignc";

	if (classmap.find("mne_alignl") == classmap.end() || classmap["mne_alignl"]
			== "")
		classmap["mne_alignl"] = "mne_alignl";

	if (classmap.find("mne_alignr") == classmap.end() || classmap["mne_alignr"]
			== "")
		classmap["mne_alignr"] = "mne_alignr";

	if (classmap.find("mne_alignj") == classmap.end() || classmap["mne_alignj"]
			== "")
		classmap["mne_alignj"] = "mne_alignj";

	if (classmap.find("mne_padding") == classmap.end()
			|| classmap["mne_padding"] == "")
		classmap["mne_padding"] = "mne_padding";

	if (classmap.find("mne_border") == classmap.end() || classmap["mne_border"]
			== "")
		classmap["mne_border"] = "mne_border";

	if (classmap.find("mne_valignt") == classmap.end()
			|| classmap["mne_valignt"] == "")
		classmap["mne_valignt"] = "mne_valignt";

	if (classmap.find("mne_valignb") == classmap.end()
			|| classmap["mne_valignb"] == "")
		classmap["mne_valignb"] = "mne_valignb";

	if (classmap.find("mne_valignm") == classmap.end()
			|| classmap["mne_valignm"] == "")
		classmap["mne_valignm"] = "mne_valignm";

	this->classmap = classmap;
}

XmlTextHtml::~XmlTextHtml()
{
}

void XmlTextHtml::mk_start()
{
	html = "";
}

void XmlTextHtml::mk_end()
{
	if (typ == O_XML)
		html = ToString::mkxml(html);
	fprintf(fp, "%s", html.c_str());
}

void XmlTextHtml::mk_text(XmlParseNode *node, int num)
{
	std::string e;
	std::map<std::string, std::string>::iterator f;

	html += "<span class=\"";

	if ( (e = node->getAttr("size")) != "")
		html += classmap["mne_font"] + e + " ";

	if (node->getAttr("weight") == "bold")
		html += classmap["mne_bold"] + " ";

	if (node->getAttr("style") == "italic")
		html += classmap["mne_italic"] + " ";

	html += "\">" + ToString::mascarade(node->getData().c_str(), "'") + "</span>";
}

void XmlTextHtml::mk_part(XmlParseNode *node, int num)
{
	std::string e;

	e = node->getAttr("align");
	if (e == "center")
		html += "<div class=\"" + classmap["mne_alignc"] + "\">";
	else if (e == "left")
		html += "<div class=\"" + classmap["mne_alignl"] + "\">";
	else if (e == "right")
		html += "<div class=\"" + classmap["mne_alignr"] + "\">";
	else
		html += "<div class=\"" + classmap["mne_alignj"] + "\">";
}

void XmlTextHtml::mk_part_end(XmlParseNode *node, int num)
{
	html += "</div>";
}

void XmlTextHtml::mk_itemize(XmlParseNode *node, int num)
{
	html += "<ul>";
}

void XmlTextHtml::mk_enumerate(XmlParseNode *node, int num)
{
	html += "<ol>";
}

void XmlTextHtml::mk_itemize_end(XmlParseNode *node, int num)
{
	html += "</ul>";
}

void XmlTextHtml::mk_enumerate_end(XmlParseNode *node, int num)
{
	html += "</ol>";
}

void XmlTextHtml::mk_item(XmlParseNode *node, int num)
{
	html += "<li>";
}

void XmlTextHtml::mk_item_end(XmlParseNode *node, int num)
{
	html += "</li>";
}

void XmlTextHtml::mk_table(XmlParseNode *node, int num)
{

	std::string e = node->getAttr("align");
	html += "<table class=\"";

	if (e == "center")
		html += " " + classmap["mne_alignc"] + " ";
	else if (e == "left")
		html += " " + classmap["mne_alignl"] + " ";
	else if (e == "right")
		html += " " + classmap["mne_alignr"] + " ";

	if (node->getAttr("padding") == "1")
		html += " " + classmap["mne_padding"] + " ";
	else
		html += " " + classmap["mne_padding"] + "no ";

	if (node->getAttr("border") == "1")
		html += " " + classmap["mne_border"] + " ";
	else
		html += " " + classmap["mne_border"] + "no ";

	html += "\"><tbody>";
}

void XmlTextHtml::mk_table_end(XmlParseNode *node, int num)
{
	html += "</tbody></table>";
}

void XmlTextHtml::mk_tabrow(XmlParseNode *node, int num)
{
	html += "<tr>";
}

void XmlTextHtml::mk_tabrow_end(XmlParseNode *node, int num)
{
	html += "</tr>";
}

void XmlTextHtml::mk_tabcol(XmlParseNode *node, int num)
{
	int i;
	std::string e;

	html += "<td class=\"";

	if (node->getAttr("padding") == "1" || act_tab->getAttr("padding") == "1")
		html += classmap["mne_padding"] + " ";
	else
		html += classmap["mne_padding"] + "no ";

	if (act_tab->getAttr("border") == "1")
		html += " " + classmap["mne_border"] + " ";
	else
	    html += " " + classmap["mne_border"] + "no ";

	e = node->getAttr("valign");
	if (e == "top")
		html += " " + classmap["mne_valignt"] + " ";
	else if (e == "bottom")
		html += " " + classmap["mne_valignb"] + " ";
	else if (e == "middle")
		html += " " + classmap["mne_valignm"] + " ";

	html += "\"";

	if (node->getAttr("relwidth") != "")
	{
		char str[128];
		sscanf(node->getAttr("relwidth").c_str(), "%d", &i);
		snprintf(str, sizeof(str), "%dpx", (int)(((double)i/100.0) * size));
		html = html + " style=\"width: " + str + "\"";
	}

	html += ">";
}

void XmlTextHtml::mk_tabcol_end(XmlParseNode *node, int num)
{
	html += "</td>";
}

