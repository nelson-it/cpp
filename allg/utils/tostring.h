#ifndef tostring_mne
#define tostring_mne

#include <string.h>
#include <string>
#include <iconv.h>

#define CHANGE_SINGLE(c,val) i=0; while ( (i = str.find(c, i) ) != std::string::npos) { str.replace(i, 1, val); i += 2; }

class ToString
{
public:
    enum CR_MASC_TYPE
    {
        MASC_CR,
        MASC_CR_NO,
        MASC_CR_CLEAR,
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

    static std::string clear(std::string str, char *sc )
    {
        std::string::size_type i = 0;
        while ( (i = str.find_first_of(sc, i) ) != std::string::npos)
            str.replace(i,1,"");
        return str;
    }

    static void mascarade(std::string &str, const char c = '\'', CR_MASC_TYPE strip = MASC_CR_NO, const char *sc = "\\n")
    {
        std::string::size_type i = 0;
        while ( (i = str.find(c, i) ) != std::string::npos)
        {
            str.insert(i, 1, '\\');
            i += 2;
        }

        if (strip == MASC_CR_CLEAR)
            sc = "";
        if (strip != MASC_CR_NO)
            for (i = 0; (i = str.find_first_of("\n", i) ) != std::string::npos;)
                str.replace(i, 1, sc);
    }

    static void mascarade(std::string &str, const char *c, CR_MASC_TYPE strip = MASC_CR_NO, const char *sc = "\\n")
    {
        std::string::size_type i = 0;
        while ( (i = str.find_first_of(c, i) ) != std::string::npos)
        {
            str.insert(i, 1, '\\');
            i += 2;
        }

        if (strip == MASC_CR_CLEAR)
            sc = "";
        if (strip != MASC_CR_NO)
            for (i = 0; (i = str.find_first_of("\n", i) ) != std::string::npos;)
                str.replace(i, 1, sc);
    }

    static std::string mascarade(const char *str_in, const char *c, CR_MASC_TYPE strip = MASC_CR_NO, const char *sc = "\\n")
    {
        std::string str(str_in);
        std::string::size_type i = 0;
        while ( (i = str.find_first_of(c, i) ) != std::string::npos)
        {
            str.insert(i, 1, '\\');
            i += 2;
        }

        if (strip == MASC_CR_CLEAR)
            sc = "";
        if (strip != MASC_CR_NO)
            for (i=0; (i = str.find_first_of("\n", i) ) != std::string::npos;)
                str.replace(i, 1, sc);
        return str;
    }

    static std::string mktex(std::string s, int ignore_backslash = 0 )
    {
        std::string str;
        std::string::size_type i = 0;

        ( ignore_backslash ) ? str = mascarade(s.c_str(), "_$&%#{}^") : str = mascarade(s.c_str(), "_$&%#{}^\\");

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

    static std::string mkxml(std::string str)
    {
        std::string::size_type i = 0;

        CHANGE_SINGLE('%', "%25");
        CHANGE_SINGLE('<', "%3C");
        CHANGE_SINGLE('>', "%3E");
        CHANGE_SINGLE('&', "%26");
        CHANGE_SINGLE('(', "%28");
        CHANGE_SINGLE(')', "%29");

        return str;
    }

    static std::string mkhtml(std::string str)
    {
        std::string::size_type i = 0;

        CHANGE_SINGLE('&', "&amp;");
        CHANGE_SINGLE(' ', "&nbsp;");
        CHANGE_SINGLE('<', "&lt;");
        CHANGE_SINGLE('>', "&gt;");

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

    static std::string mkjson(std::string str)
    {
        std::string::size_type i = 0;

        CHANGE_SINGLE('\\', "\\\\");
        CHANGE_SINGLE('"', "\\\"");
        CHANGE_SINGLE('/', "\\/");
        CHANGE_SINGLE('\b', "\\b");
        CHANGE_SINGLE('\f', "\\f");
        CHANGE_SINGLE('\n', "\\n");
        CHANGE_SINGLE('\r', "\\r");
        CHANGE_SINGLE('\t', "\\t");

        return str;
    }

    static std::string substitute(std::string s, const char *from = "/", const char *to = "\\")
    {
        std::string str = s;
        std::string::size_type i = 0;
        std::string::size_type fl = strlen(from);
        std::string::size_type tl = strlen(to);

        while ( (i = str.find(from, i) ) != std::string::npos)
        {
            str.replace(i, fl, to);
            i += tl;
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

    static std::string to_utf8( std::string str, const char *encoding = "iso-8859-1" )
    {
        iconv_t iv;
        char *inbuf, *outbuf, *ci, *co;
        size_t innum,outnum;

        ci = inbuf = (char *)str.c_str();
        innum = str.length();

        co = outbuf = new char[str.size() * 4];
        outnum = ( str.size() * 4 - 1);

        iv = iconv_open("utf-8//TRANSLIT", encoding);
        iconv (iv, &ci, &innum, &co, &outnum);
        iconv_close(iv);

        *co = '\0';
        str = outbuf;
        delete[] outbuf;

        return str;
    }

    static std::string from_utf8( std::string str, const char *encoding = "iso-8859-1" )
    {
        iconv_t iv;
        char *inbuf, *outbuf, *ci, *co;
        size_t innum,outnum;

        ci = inbuf = (char *)str.c_str();
        innum = str.length();

        co = outbuf = new char[str.size() * 4];
        outnum = ( str.size() * 4 - 1);

        iv = iconv_open( (std::string(encoding) + "//TRANSLIT").c_str(), "utf-8");
        if ( iv != (iconv_t) -1 )
        {
            iconv (iv, &ci, &innum, &co, &outnum);
            iconv_close(iv);
            *co = '\0';
            str = outbuf;
        }

        delete[] outbuf;
        return str;
    }

};

#endif /* tostring_mne */
