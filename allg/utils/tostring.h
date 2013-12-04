#ifndef tostring_mne
#define tostring_mne

#include <string>

class ToString
{
public:
    enum STRIP_TYPE
    {
        STRIP_NO,
        STRIP_CLEAR,
        STRIP_CHANGE
    };

    static std::string convert(long i, const char *format = "%ld")
    {
        char str[128];
        snprintf(str, 128, format, i);
        str[127] = '\0';
        return str;
    }

    static std::string convert(double d, const char *format = "%f")
    {
        char str[128];
        snprintf(str, 128, format, d);
        str[127] = '\0';
        return str;
    }

    static std::wstring s2w(std::string s)
    {
        std::wstring temp(s.length(),L' ');
        std::copy(s.begin(), s.end(), temp.begin());
        return temp;
    }

    static std::string w2s(std::wstring s)
    {
        std::string temp(s.length(), ' ');
        std::copy(s.begin(), s.end(), temp.begin());
        return temp;
    }

    static void mascarade(std::string &str, const char c = '\'',
            STRIP_TYPE strip = STRIP_NO, const char *sc = "\\n")
    {
        std::string::size_type i = 0;
        while ( (i = str.find(c, i) ) != std::string::npos)
        {
            str.insert(i, 1, '\\');
            i += 2;
        }

        if (strip == STRIP_CLEAR)
            sc = "";
        if (strip != STRIP_NO)
            for (i = 0; (i = str.find_first_of("\n", i) ) != std::string::npos;)
                str.replace(i, 1, sc);
    }

    static std::string clear(std::string str, char *sc )
    {
        std::string::size_type i = 0;
        while ( (i = str.find_first_of(sc, i) ) != std::string::npos)
            str.replace(i,1,"");
        return str;
    }

    static void mascarade(std::string &str, const char *c,
            STRIP_TYPE strip = STRIP_NO, const char *sc = "\\n")
    {
        std::string::size_type i = 0;
        while ( (i = str.find_first_of(c, i) ) != std::string::npos)
        {
            str.insert(i, 1, '\\');
            i += 2;
        }

        if (strip == STRIP_CLEAR)
            sc = "";
        if (strip != STRIP_NO)
            for (i = 0; (i = str.find_first_of("\n", i) ) != std::string::npos;)
                str.replace(i, 1, sc);
    }

    static std::string mascarade(const char *str_in, const char *c,
            STRIP_TYPE strip = STRIP_NO, const char *sc = "\\n")
    {
        std::string str(str_in);
        std::string::size_type i = 0;
        while ( (i = str.find_first_of(c, i) ) != std::string::npos)
        {
            str.insert(i, 1, '\\');
            i += 2;
        }

        if (strip == STRIP_CLEAR)
            sc = "";
        if (strip != STRIP_NO)
            for (i=0; (i = str.find_first_of("\n", i) ) != std::string::npos;)
                str.replace(i, 1, sc);
        return str;
    }

    static std::string mktex(std::string s)
    {
        std::string str = mascarade(s.c_str(), "_$&%#{}^\\");
        std::string::size_type i = 0;

        while ( (i = str.find('<', i) ) != std::string::npos)
        {
            str.replace(i, 1, "\\textless ");
            i += 4;
            if ( str[i] == ' ') { str.insert(i,"\\"); i++; }
        }

        i = 0;
        while ( (i = str.find('>', i) ) != std::string::npos)
        {
            str.replace(i, 1, "\\textgreater ");
            i += 4;
            if ( str[i] == ' ') { str.insert(i,"\\"); i++; }
        }

        i = 0;
        while ( (i = str.find('"', i) ) != std::string::npos)
        {
            str.replace(i, 1, "\\grqq ");
            i += 6;
            if ( str[i] == ' ') { str.insert(i,"\\"); i++; }
        }


        return str;
    }

#define MKXML_SINGLE(c,val) i=0; while ( (i = str.find(c, i) ) != std::string::npos) { str.replace(i, 1, val); i += 2; }
    static std::string mkxml(std::string str)
    {
        std::string::size_type i = 0;

        MKXML_SINGLE('%', "%25");
        MKXML_SINGLE('<', "%3C");
        MKXML_SINGLE('>', "%3E");
        MKXML_SINGLE('&', "%26");
        MKXML_SINGLE('(', "%28");
        MKXML_SINGLE(')', "%29");

        return str;
    }

    static std::string mkhtml(std::string str)
    {
        std::string::size_type i = 0;

        MKXML_SINGLE('&', "&amp;");
        MKXML_SINGLE(' ', "&nbsp;");
        MKXML_SINGLE('<', "&lt;");
        MKXML_SINGLE('>', "&gt;");

        return str;
    }

    static std::string mktexmacro(std::string s)
    {
        std::string str = s;
        std::string::size_type i = 0;
        while ( (i = str.find('_', i) ) != std::string::npos)
        {
            str.replace(i, 1, "");
            i += 4;
        }

        return str;
    }

    static std::string mkcsv(std::string s)
    {
        std::string str = s;
        std::string::size_type i = 0;
        while ( (i = str.find('"', i) ) != std::string::npos)
        {
            str.replace(i, 1, "\"\"");
            i += 2;
        }

        return str;
    }

    static int pattern( const char *string, const char *pattern)
    {
        const char *str = NULL, *ptr = NULL;

        while ((*string) && (*pattern != '*'))
        {
            if ((*pattern != *string) && (*pattern != '?'))
            {
                return 0;
            }
            pattern++;
            string++;
        }

        while (*string)
        {
            if (*pattern == '*')
            {
                if ((*++pattern) == '\0' )
                    return 1;
                ptr = pattern;
                str = string+1;
            }
            else if ((*pattern == *string) || (*pattern == '?'))
            {
                pattern++;
                string++;
            }
            else
            {
                pattern = ptr;
                string = str++;
            }
        }

        while (*pattern == '*')
            pattern++;

        return ( *pattern == '\0' );
    }

};

#endif /* tostring_mne */
