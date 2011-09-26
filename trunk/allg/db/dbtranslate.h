#ifndef dbtranslate_mne
#define dbtranslate_mne

#include <map>
#include <string>

#include <message/message.h>

class Database;
class DbTranslate : public MessageTranslator
{
    #ifdef PTHREAD
    static pthread_mutex_t mutex;
    #endif

    static Database *db;
    std::string lang;
    std::string region;
    typedef std::map<std::string, std::map<std::string,std::string> > CacheMap;
    static CacheMap cache;

    typedef std::map<std::string, std::string > FormatMap;
    static FormatMap date;
    static FormatMap time;
    static FormatMap interval;
    static FormatMap times;
    static FormatMap intervals;

    static int init;

    std::string act_date;
    std::string act_time;
    std::string act_interval;
    std::string act_times;
    std::string act_intervals;

public:
    DbTranslate(Database *db, std::string lang = "de", std::string region = "CH" );
    virtual ~DbTranslate();

    std::string getLang()                 { return lang; }
    void        setLang(std::string lang);

    std::string getRegion()                 { return region; }
    void        setRegion(std::string region);

    std::string getDateformat()      { return act_date; }
    std::string getTimeformat()      { return act_time; }
    std::string getIntervalformat()  { return act_interval; }
    std::string getTimesformat()     { return act_times; }
    std::string getIntervalsformat() { return act_intervals; }

    std::string get( const char *str, const char *kategorie = "" );
    std::string get( std::string str, const char *kategorie = "" )
        { return get(str.c_str(), kategorie); }

   void clear_cache();
};

#endif /* dbtranslate_mne */

