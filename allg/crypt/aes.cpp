#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "aes.h"

Aes::Aes()
{
    passwd.push_back('X');
    passwd.push_back('a');
    passwd.push_back('t');
    passwd.push_back('r');
    passwd.push_back('o');
    passwd.push_back('4');
    passwd.push_back('w');
    passwd.push_back('p');
    passwd.push_back('y');
    passwd.push_back('X');
    passwd.push_back('J');
    passwd.push_back('5');
    passwd.push_back('^');
    passwd.push_back('*');
    passwd.push_back('8');
    passwd.push_back('P');
    passwd.push_back('P');
    passwd.push_back('E');
    passwd.push_back('t');
    passwd.push_back('r');
    passwd.push_back('2');
    passwd.push_back('b');
    passwd.push_back('/');
    passwd.push_back('.');
    passwd.push_back('t');
    passwd.push_back('[');
    passwd.push_back(';');
    passwd.push_back('5');
    passwd.push_back('1');
    passwd.push_back('G');
    passwd.push_back('X');
}

std::string Aes::encode(std::string in)
{
    unsigned char *r;
    unsigned char *b;
    unsigned int  num;
    unsigned int l1, l2;
    std::string ret;

    l1 = in.length() + 16;
    l2 = ((( in.length() + 16) / 3 ) * 4) + 5;
    r = new unsigned char[l1];
    b = new unsigned char[l2];

    rijndael.init(Rijndael::ECB, Rijndael::Encrypt,
    (unsigned char*)passwd.c_str(), Rijndael::Key32Bytes);

    num = rijndael.padEncrypt((unsigned char*)(in + "=").c_str(),
                              in.length(), r);

    if ( num > 0 )
    {
	num = base64.encode(r, b, num);
	b[num - 1] = '\0';
    }
    else
       *b = '\0';

    ret = (char *)b;
    delete r;
    delete b;

    return ret;
}

std::string Aes::decode(std::string in)
{
    unsigned char *r;
    unsigned char *b;
    unsigned int  num;
    std::string ret;

    if ( in.length() == 0 ) return "";

    r = new unsigned char[in.length() + 1];
    b = new unsigned char[in.length() + 2];

    rijndael.init(Rijndael::ECB, Rijndael::Decrypt,
    (unsigned char*)passwd.c_str(), Rijndael::Key32Bytes);

    num = base64.decode((unsigned char*)in.c_str(), b, in.length());
    num = ( num / 16 ) * 16;
    num = rijndael.padDecrypt(b, num, r);
    r[num] = '\0';

    ret = (char *)r;
    delete r;
    delete b;

    return ret;
}


