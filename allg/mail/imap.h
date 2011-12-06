#ifndef imap_mne
#define imap_mne

#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <string>
#include <vector>

#include <message/message.h>

class Imap
{
    enum ErrorTypes
    {
        E_OK = 0,
        E_OPEN,
        E_HOST,
        E_CONNECT,
        E_WRITE,
        E_READ
    };

public:

    typedef std::map<std::string,std::string> Folder;
    typedef std::map<std::string, std::string> Header;
    typedef std::map<std::string, Header> Headers;
    typedef std::vector<std::string> Answer;
private:
    Message msg;

    int sock;

    std::string tag;
    int cmd_count;
    Answer answer;
    char *response;

    int bracket_found;
    std::string num;

    CsList split(std::string str);
    int write_cmd(char *cmd, int len, int need_split = 1);
    void read_answer(int need_split = 1);

    std::string getBracketvalue(std::string s, const char *delimiter = "()");

    void scan_bodystructure(std::string s, Header *h, std::string pre);
    std::string mk_utf8(std::string str);

public:
    Imap(std::string server = "", std::string user = "", std::string passwd = "");
    ~Imap();

    void connect(std::string server, std::string user, std::string passwd);

    std::string getHeaderElement(Header *h, std::string name )
    {
        Imap::Header::iterator i;
        i = h->find(name);
           if ( i != h->end() )
               return i->second;
           else
               return "";
    }

    std::string getAddress(std::string str)
    {
        std::string::size_type j = str.find("<");
         if ( j != std::string::npos)
             return str.substr(j + 1, str.find(">") - j - 1);
         else
             return str;
    }
    std::string getName(std::string str)
    {
        std::string::size_type j = str.find("<");
         if ( j != std::string::npos)
         {
             while( j > 0 && str[j-1] == ' ' ) j--;
             return str.substr(0, j );
         }
         else
             return str;
    }

    Folder getFolder();

    Headers getHeader(std::string mbox, time_t t);
    std::string getText(std::string uid);

};

#endif /* imap_mne */
