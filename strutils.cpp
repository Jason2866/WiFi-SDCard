
#include "strutils.h"

#include <StreamString.h>

// define cal constants
const char *months[]  = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char *wdays[]  = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

int htoi(char c)
{
    c = tolower(c);
    return c >= '0' && c <= '9' ? c - '0' :
           c >= 'a' && c <= 'f' ? c - 'a' + 10 :
           -1;
}


char itoH(int c)
{
    return c <= 9 ? c + '0' : c - 10 + 'A';
}


int hhtoi(const char* c)
{
    int h = htoi(*c);
    int l = htoi(*(c + 1));
    return h < 0 || l < 0 ? -1 : (h << 4) + l;
}


bool notEncodable (char c)
{
    return c > 32 && c < 127;
}


void stripSlashes(String& name)
{
    size_t i = 0;
    while (i < name.length())
        if (name[i] == '/' && name.length() > 1 && ((i == name.length() - 1) || name[i + 1] == '/'))
            name.remove(i, 1);
        else
            i++;
}


#if STREAMSEND_API

String date2date(time_t date)
{
    // get & convert time to required format
    // Tue, 13 Oct 2015 17:07:35 GMT
    tm* gTm = gmtime(&date);
    String ret;
    ret.reserve(40);
    S2Stream(ret).printf("%s, %02d %s %04d %02d:%02d:%02d GMT",
        wdays[gTm->tm_wday],
        gTm->tm_mday,
        months[gTm->tm_mon],
        gTm->tm_year + 1900,
        gTm->tm_hour,
        gTm->tm_min,
        gTm->tm_sec);
    return ret;
}

#else // !STREAMSEND_API

String date2date(time_t date)
{
    // get & convert time to required format
    // Tue, 13 Oct 2015 17:07:35 GMT
    tm* gTm = gmtime(&date);
    char buf[40];
    snprintf(buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d GMT",
        wdays[gTm->tm_wday],
        gTm->tm_mday,
        months[gTm->tm_mon],
        gTm->tm_year + 1900,
        gTm->tm_hour,
        gTm->tm_min,
        gTm->tm_sec);
    return buf;
}

#endif // !STREAMSEND_API


String OLDenc2c(const String& encoded)
{
    String ret = encoded;
    for (size_t i = 0; ret.length() >= 2 && i < ret.length() - 2; i++)
        if (ret[i] == '%')
        {
            int v = hhtoi(ret.c_str() + i + 1);
            if (v > 0)
            {
                ret[i] = v < 128 ? (char)v : '=';
                ret.remove(i + 1, 2);
            }
        }
    return ret;
}


String enc2c(const String& encoded)
{
    int v;
    String ret;
    ret.reserve(encoded.length());
    for (size_t i = 0; i < encoded.length(); i++)
    {
        if (   encoded[i] == '%'
            && (i + 3) <= encoded.length()
            && (v = hhtoi(encoded.c_str() + i + 1)) > 0)
        {
            ret += v;
            i += 2;
        }
        else
            ret += encoded[i];
    }
    return ret;
}


String c2enc(const String& decoded)
{
    size_t l = decoded.length();
    for (size_t i = 0; i < decoded.length(); i++)
        if (!notEncodable(decoded[i]))
            l += 2;

    String ret;
    ret.reserve(l);
    for (size_t i = 0; i < decoded.length(); i++)
    {
        char c = decoded[i];
        if (notEncodable(c))
            ret += c;
        else
        {
            ret += '%';
            ret += itoH(c >> 4);
            ret += itoH(c & 0xf);
        }
    }
    return ret;
}


void replaceFront (String& str, const String& from, const String& to)
{
    if (from.length() && to.length() && str.indexOf(from) == 0)
    {
        String repl;
        repl.reserve(str.length() + to.length() - from.length() + 1);
        repl = to;
        size_t skip = from.length() == 1? 0: from.length();
        repl += str.c_str() + skip;
        str = repl;
        stripSlashes(str);
    }
}

String urlToUri(const String& url)
{
    int index;
    if (url.startsWith("http") && (index = url.indexOf("://")) <= 5)
    {
        int uriStart = url.indexOf('/', index + 3);
        return url.substring(uriStart);
    }
    return url;
}

