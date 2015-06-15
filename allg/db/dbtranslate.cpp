#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#if defined(__CYGWIN__) || defined(__MINGW32__)
#include <windows.h>
#endif

#include <argument/argument.h>

#include "dbconstraint_error.h"
#include "dbtranslate.h"
#include "database.h"
#include "dbtable.h"

DbTranslate::CacheMap DbTranslate::cache;
DbTranslate::FormatMap DbTranslate::date;
DbTranslate::FormatMap DbTranslate::time;
DbTranslate::FormatMap DbTranslate::interval;
DbTranslate::FormatMap DbTranslate::times;
DbTranslate::FormatMap DbTranslate::intervals;
int DbTranslate::init = 0;

Database *DbTranslate::db = NULL;
pthread_mutex_t DbTranslate::mutex;

DbTranslate::DbTranslate(Database *db, std::string lang, std::string region)
{
    if (this->db == NULL)
    {
        Argument a;
        this->db = db->getDatabase();
        this->db->p_getConnect("", a["DbTranslateUser"], a["DbTranslatePasswd"]);
        while ( ! this->db->have_connection() )
        {
#if defined(__MINGW32__) || defined(__CYGWIN__)
            Sleep(10000);
#else
            sleep(10);
#endif
            this->db->p_getConnect("", a["DbTranslateUser"], a["DbTranslatePasswd"]);
        }

        DbConstraintError e;
        e.read(db);
    }

    if ( ! init )
    {
        DbConnect::ResultMat *r;
        DbConnect::ResultMat::iterator rm;
        CsList cols("language,region,typ,style");
        DbTable *tab = this->db->p_getTable(db->getApplschema(), "timestyle");
        r = tab->select(&cols, NULL);
        for ( rm = r->begin(); rm != r->end(); ++rm )
        {
            if ( (std::string)((*rm)[2]) == "date")
            {
                date[((std::string)((*rm)[0])) + "_" + ((std::string)((*rm)[1]))] = (std::string)((*rm)[3]);
            }
            else if ( (std::string)((*rm)[2]) == "time")
            {
                time[((std::string)((*rm)[0])) + "_" + ((std::string)((*rm)[1]))] = (std::string)((*rm)[3]);
            }
            else if ( (std::string)((*rm)[2]) == "interval")
            {
                interval[((std::string)((*rm)[0])) + "_" + ((std::string)((*rm)[1]))] = (std::string)((*rm)[3]);
            }
            else if ( (std::string)((*rm)[2]) == "times")
            {
                times[((std::string)((*rm)[0])) + "_" + ((std::string)((*rm)[1]))] = (std::string)((*rm)[3]);
            }
            else if ( (std::string)((*rm)[2]) == "intervals")
            {
                intervals[((std::string)((*rm)[0])) + "_" + ((std::string)((*rm)[1]))] = (std::string)((*rm)[3]);
            }
        }
        tab->end();
        this->db->release(tab);
    }

    act_date = "%d.%m.%Y";
    act_time = "%H:%M:%S";
    act_interval = "%02d:%02d:%02d";
    act_times = "%H:%M";
    act_intervals= "%02d:%02d";

    this->lang = lang;
    this->setRegion(region);

#ifdef PTHREAD
    pthread_mutex_init(&mutex,NULL);
#endif
}

DbTranslate::~DbTranslate()
{
    delete this->db;
    this->db = NULL;
}

void DbTranslate::setLang(std::string lang)
{
    this->lang = lang;
    FormatMap::iterator i;

    if ( ( i = date.find(this->lang + "_" + this->region)) != date.end() )
         act_date = i->second;
    else if ( ( i = date.find(this->lang)) != date.end() )
         act_date = i->second;
    else if ( ( i = date.find("")) != date.end() )
         act_date = i->second;

    if ( ( i = time.find(this->lang + "_" + this->region)) != time.end() )
         act_time = i->second;
    else if ( ( i = time.find(this->lang)) != time.end() )
         act_time = i->second;
    else if ( ( i = time.find("")) != time.end() )
         act_time = i->second;

    if ( ( i = interval.find(this->lang + "_" + this->region)) != interval.end() )
         act_interval = i->second;
    else if ( ( i = interval.find(this->lang)) != interval.end() )
         act_interval = i->second;
    else if ( ( i = interval.find("")) != interval.end() )
         act_interval = i->second;

    if ( ( i = times.find(this->lang + "_" + this->region)) != times.end() )
         act_times = i->second;
    else if ( ( i = times.find(this->lang)) != times.end() )
         act_times = i->second;
    else if ( ( i = times.find("")) != times.end() )
         act_times = i->second;

    if ( ( i = intervals.find(this->lang + "_" + this->region)) != intervals.end() )
         act_intervals = i->second;
    else if ( ( i = intervals.find(this->lang)) != intervals.end() )
         act_intervals = i->second;
    else if ( ( i = intervals.find("")) != intervals.end() )
         act_intervals = i->second;

}

void DbTranslate::setRegion(std::string region)
{
    this->region = region;
    FormatMap::iterator i;

    if ( ( i = date.find(this->lang + "_" + this->region)) != date.end() )
         act_date = i->second;
    else if ( ( i = date.find(this->lang)) != date.end() )
         act_date = i->second;
    else if ( ( i = date.find("")) != date.end() )
         act_date = i->second;

    if ( ( i = time.find(this->lang + "_" + this->region)) != time.end() )
         act_time = i->second;
    else if ( ( i = time.find(this->lang)) != time.end() )
         act_time = i->second;
    else if ( ( i = time.find("")) != time.end() )
         act_time = i->second;

    if ( ( i = interval.find(this->lang + "_" + this->region)) != interval.end() )
         act_interval = i->second;
    else if ( ( i = interval.find(this->lang)) != interval.end() )
         act_interval = i->second;
    else if ( ( i = interval.find("")) != interval.end() )
         act_interval = i->second;

    if ( ( i = times.find(this->lang + "_" + this->region)) != times.end() )
         act_times = i->second;
    else if ( ( i = times.find(this->lang)) != times.end() )
         act_times = i->second;
    else if ( ( i = times.find("")) != times.end() )
         act_times = i->second;

    if ( ( i = intervals.find(this->lang + "_" + this->region)) != intervals.end() )
         act_intervals = i->second;
    else if ( ( i = intervals.find(this->lang)) != intervals.end() )
         act_intervals = i->second;
    else if ( ( i = intervals.find("")) != intervals.end() )
         act_intervals = i->second;


}

std::string DbTranslate::get(const char *str, const char *kategorie)
{

    static int in_get = 0;

    if (!db->have_connection()) return str;

    if (in_get && db->p_getConnect()->get_error()) return str;

    in_get = 1;

#ifdef PTHREAD
    pthread_mutex_lock(&mutex);
#endif

    std::string stm, s;
    char *c;
    int result;
    std::map<std::string, std::map<std::string, std::string> >::iterator il;
    std::map<std::string, std::string>::iterator is;

    if ((il = cache.find(this->lang)) != cache.end())
    {
        if ((is = il->second.find(str)) != il->second.end())
        {
#ifdef PTHREAD
            pthread_mutex_unlock(&mutex);
#endif

            return is->second;
        }
    }

    if (il == cache.end())
    {
        std::map < std::string, std::string > l;
        cache[this->lang] = l;
        il = cache.find(this->lang);
    }

    s = str;
    db->p_getConnect()->mk_string(s);
    stm = "SELECT text_" + this->lang + " FROM " + db->getApplschema()
            + ".translate " + " WHERE id = " + s;

    result = db->p_getConnect()->execute(stm.c_str(), 1);
    if (db->p_getConnect()->have_result())
    {
        c = (char *) (*db->p_getConnect()->p_get_first_result())[0];
        in_get = 0;
        if (*c == '\0')
        {
#ifdef PTHREAD
            pthread_mutex_unlock(&mutex);
#endif

            return str;
        }
        else
        {
            il->second[str] = c;

#ifdef PTHREAD
            pthread_mutex_unlock(&mutex);
#endif

            return c;
        }
    }
    else if (result == 0)
    {

        DbTable::ValueMap vals;
        DbTable *tab;

        in_get = 1;
        tab = db->p_getTable(db->getApplschema(), "translate");
        vals["id"] = str;
        vals["categorie"] = kategorie;
        tab->insert(&vals, 1);
        db->release(tab);
        in_get = 0;
    }

    in_get = 0;

#ifdef PTHREAD
    pthread_mutex_unlock(&mutex);
#endif

    return str;
}

void DbTranslate::clear_cache()
{
#ifdef PTHREAD
    pthread_mutex_lock(&mutex);
#endif

    CacheMap c;
    cache = c;

#ifdef PTHREAD
    pthread_mutex_unlock(&mutex);
#endif
}

