#include <pthread.h>
#include "stdlib.h"
#include "stdio.h"

#include "xmlparse.h"


XmlParseNode::XmlParseNode(Message *msg, XmlParseNode *parent)
{
    this->msg = msg;
    this->parent = parent;
}

XmlParseNode::~XmlParseNode()
{
    unsigned int i;
    for ( i=0; i<childs.size(); ++i) delete childs[i];
}

int XmlParseNode::setXml( std::string stag, std::string content)
{

    std::string::size_type i, pos1, pos2, spos, epos;
    int result;
    std::string nexttag,nextid, nextcontent;

    for ( i=0; i<childs.size(); ++i) delete childs[i];
    childs.clear();

    if ( stag != "" && stag != "<>" )
        if ( ( result = parse_attr(stag)) != 0 )
            return result;

    while( content.size() != 0 )
    {
        spos = content.find('<');
        if ( spos == std::string::npos )
        {
            data += content;
            return 0;
        }

        if ( spos != 0 )
        {
            data += content.substr(0, spos );
        }

        if ( content[spos + 1] == '/' )
        {
            msg->perror(E_TAGNOTOPEN, "Endtag ohne Starttag gefunden %s", content.substr(spos, 20).c_str());
            return -E_TAGNOTOPEN;
        }

        epos = content.find_first_of("<>/", spos + 1);
        if ( epos == std::string::npos || content[epos] == '<' )
        {
            msg->perror(E_TAGNOTCLOSE, "Das Element <%s> ist nicht geschlossen", content.substr(pos1, 20).c_str());
            return -E_TAGNOTCLOSE;
        }

        nexttag = content.substr(spos, epos - spos + 1 );
        spos++;
        pos1 = content.find_first_of(" /\t\n\r>", spos);
        nextid = content.substr(spos, pos1 - spos);
        if ( content[epos] != '/' )
        {
            pos2 = content.find("</" + nextid + ">", epos + 1);
            if ( pos2 == std::string::npos )
            {
                msg->perror(E_NOENDTAG, "Das Element <%s> besitzt keinen Endtag", nextid.c_str());
                msg->line ("%s", content.substr(pos1 + 1).c_str());
                return -E_NOENDTAG;
            }
            else
            {
                pos1 = content.find("<" + nextid , epos + 1);
                while ( pos1 != std::string::npos && isalpha(content[pos1 + nextid.length() + 1]) && content[pos1 + nextid.length() + 1] != '>' ) pos1 = content.find("<" + nextid , pos1 + 1);
                while ( pos1 != std::string::npos && 
                        ( isspace(content[pos1 + nextid.length() + 1]) ||
                                content[pos1 + nextid.length() + 1] == '>' ) &&
                                pos1 < pos2 )
                {
                    pos2 = content.find("</" + nextid + ">", pos2 + 1);
                    pos1 = content.find("<" + nextid ,  pos1 + 1);
                    while ( pos1 != std::string::npos && isalpha(content[pos1 + nextid.length() + 1])  && content[pos1 + nextid.length() + 1] != '>' ) pos1 = content.find("<" + nextid , pos1 + 1);
                }

                nextcontent = content.substr(epos + 1, pos2 - epos - 1);
                pos2 = pos2 + nextid.length() + 3;
            }

        }
        else
        {
            nextcontent = "";
            nexttag = "<" + nextid + ">";
            pos2 = epos + 2;
        }

        if ( nexttag != "" || nextcontent != "" )
        {
            XmlParseNode *node = new XmlParseNode(msg, this);
            int r;

            r = node->setXml(nexttag, nextcontent);
            if ( r < 0 ) return r;

            childs.push_back(node);
        }

        content = content.substr(pos2);
    }

    if ( childs.size() )
        childs[childs.size() -1 ]->attrs["lastchild"] = "1";

    return 0;
}

int XmlParseNode::parse_attr(std::string stag)
{

    std::string::size_type pos1, pos2;

    attrs.clear();
    pos1 = stag.find('<');
    pos2 = stag.find_first_of(" \t\r\n>", ++pos1);

    id = stag.substr( pos1, pos2 - pos1 ); 
    stag = stag.substr(pos2);
    pos2 = 0;
    while ( pos2 < stag.length() - 1)
    {
        std::string attr,value;

        pos1 = stag.find_first_not_of(" \t\r\n", pos2 );
        if ( pos1 == std::string::npos )
        {
            msg->perror(E_ARGUMENT, "Argumentenliste in Element <%s> ist fehlerhaft", id.c_str());
            return -E_ARGUMENT;
        }


        pos2 = stag.find_first_of("=", pos1);

        if ( pos2 == std::string::npos )
        {
            msg->perror(E_ARGUMENT, "Argumentenliste in Element <%s> ist fehlerhaft", id.c_str());
            return -E_ARGUMENT;
        }

        attr = stag.substr(pos1, pos2 - pos1);
        pos1 = pos2 + 1;

        if ( stag[pos1] != '\'' && stag[pos1] != '"' )
        {
            msg->perror(E_DELIMITER, "Attributwert von Attribut <%s> in Element <%s> begin nicht mit einem erlaubten Begrenzer", attr.c_str(), id.c_str());
            return -E_DELIMITER;
        }

        pos2 = stag.find(stag[pos1], pos1 + 1);
        if ( pos2 == std::string::npos )
        {
            msg->perror(E_DELIMITER, "Attributwert von Attribut <%s> in Element <%s> endet nicht mit dem richtigen Begrenzer", attr.c_str(), id.c_str());
            return -E_DELIMITER;
        }
        if ( stag.find_first_of(" \t\r\n>", pos2 ) != (pos2 + 1) )
        {
            msg->perror(E_DELIMITER, "Attributwert von Attribut <%s> in Element <%s> beinhaltet seinen Begrenzer", attr.c_str(), id.c_str());
            return -E_DELIMITER;
        }

        value = stag.substr(pos1+1, pos2 - pos1 - 1);
        pos2++;

        attrs[attr] = value;
    }

    return 0;

}

XmlParse::XmlParse()
:msg("XmlParse"),
 root(&msg, NULL)
{
    error_found = 0;
}

XmlParse::~XmlParse()
{
}


int XmlParse::setXml(std::string xml)
{
    if ( xml[0] != '<' )
        xml = "<part><text size=\"n\">" + xml + "</text></part>";

    error_found = root.setXml("", xml);
    return error_found;
}

