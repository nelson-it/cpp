#ifndef aes_mne
#define aes_mne

#include "base64.h"
#include "rijndael.h"

class Aes
{
     CryptBase64 base64;
     Rijndael    rijndael;

     std::string passwd;
public:
    Aes();
    ~Aes() {};

    std::string encode(std::string);
    std::string decode(std::string);

    void setPasswd(const char *passwd)
    { this->passwd = passwd; }
};

inline std::string aes_encode(std::string in, std::string passwd)
{
    Aes aes;
    std::string ret;
    aes.setPasswd(passwd.c_str());
    ret = aes.encode(in);
    return ret;
}

inline std::string aes_decode(std::string in, std::string passwd)
{
    Aes aes;
    std::string ret;
    aes.setPasswd(passwd.c_str());
    ret = aes.decode(in);
    return ret;
}

#endif /* aes_mne */

