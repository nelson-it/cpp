#ifndef base64_mne
#define base64_mne

#include <string>

class CryptBase64
{
    static const char *base64tab;
    char *rbase64tab;

public:
    CryptBase64();
    ~CryptBase64();

    std::string encode( std::string in );
    std::string decode( std::string in);

    int encode( const unsigned char *in, unsigned char *out, int len );
    int decode( const unsigned char *in, unsigned char *out, int len );

    int encode_needsize(int len);

};

#endif

