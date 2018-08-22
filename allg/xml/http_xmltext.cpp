#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <xml/xmltext_html.h>
#include <xml/xmltext_htmledit.h>

#include "http_xmltext.h"

HttpXmlText::HttpXmlText(Http *h) :
	HttpProvider(h), msg("HttpXmlText")
{
		subprovider["html.html"] = &HttpXmlText::html;
		subprovider["html.js"]   = &HttpXmlText::htmledit;

		h->add_provider(this);
}

HttpXmlText::~HttpXmlText()
{
}

int HttpXmlText::request(HttpHeader *h)
{
	SubProviderMap::iterator i;

	if ( (i = subprovider.find(h->filename)) != subprovider.end() )
	{
		(this->*(i->second))(h);
		return 1;
	}

	return 0;
}



void HttpXmlText::html(HttpHeader *h)
{
	std::map<std::string,std::string> classmap;
    std::string str =  h->vars["xmltext"];

	classmap["mne_font"] = h->vars["mne_font"];
	classmap["mne_bold"] = h->vars["mne_bold"];
	classmap["mne_italic"] = h->vars["mne_italic"];

	classmap["mne_alignc"] = h->vars["mne_alignc"];
	classmap["mne_alignc"] = h->vars["mne_alignc"];
	classmap["mne_alignl"] = h->vars["mne_alignl"];
	classmap["mne_alignr"] = h->vars["mne_alignr"];
	classmap["mne_alignj"] = h->vars["mne_alignj"];

	classmap["mne_padding"] = h->vars["mne_padding"];

	classmap["mne_valignt"] = h->vars["mne_valignt"];
	classmap["mne_valignb"] = h->vars["mne_valignb"];
	classmap["mne_valignm"] = h->vars["mne_valignm"];

	XmlTextHtml xml(h->vars["size"], classmap);

	h->content_type = "text/plain";
	h->status = 200;

	//if ( str[0] != '<' )
	//    str = "<part><text size=\"n\">" + str + "</text></part>";

	xml.setXml(str);
	xml.mk_output();
	add_content(h, xml.get_string(), strlen(xml.get_string()));
}

void HttpXmlText::htmledit(HttpHeader *h)
{
	XmlTextHtmlEdit xml(h->vars["editor"]);

	h->content_type = "text/plain";
	h->status = 200;
	xml.setXml(h->vars["xmltext"]);
	xml.mk_output();
	add_content(h, xml.get_string(), strlen(xml.get_string()));
}

