#ifdef PTHREAD
#include <pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <string.h>

#include <openssl/evp.h>

#define MIN(x,y) (x<y) ? x : y

std::string encode_cipher_base64(char *name, const char *in, const char *passwd)
{
    const EVP_CIPHER *cipher;
    EVP_CIPHER_CTX ctx;
    unsigned char key[EVP_MAX_KEY_LENGTH];
    unsigned char *ciphertext;
    unsigned char *base64text;
    std::string ret;
    int i,len;

    memset(key, 0, EVP_MAX_KEY_LENGTH);
    memcpy(key, passwd, MIN(EVP_MAX_KEY_LENGTH, strlen(passwd)));

    cipher = EVP_get_cipherbyname(name);
    if ( cipher == NULL )
    {
        OpenSSL_add_all_ciphers();
        cipher = EVP_get_cipherbyname(name);
        if ( cipher == NULL ) return "";
    }

    EVP_EncryptInit(&ctx, cipher, key, NULL);
    ciphertext = new unsigned char[strlen(in) + EVP_CIPHER_CTX_block_size(&ctx) + 1];

    EVP_EncryptUpdate(&ctx, ciphertext, &len, (unsigned char*)in, strlen(in));
    EVP_EncryptFinal(&ctx, &ciphertext[len], &i);
    len += i;

    base64text = new unsigned char[(((len + 2) / 3) * 4) + 5];
    sprintf((char*)base64text, "%04d", len);
    EVP_EncodeBlock(&base64text[4], ciphertext, len);
    base64text[(((len + 2) / 3) * 4) + 4] = '\0';
    ret = (char *)base64text;

    delete ciphertext;
    delete base64text;

    return ret;
 }

std::string decode_cipher_base64(char *name, const char *in, const char *passwd)
{
    const EVP_CIPHER *cipher;
    EVP_CIPHER_CTX ctx;
    unsigned char iv[EVP_MAX_IV_LENGTH];
    unsigned char key[EVP_MAX_KEY_LENGTH];
    unsigned char *ciphertext;
    unsigned char *base64text;
    char len_str[5];
    std::string ret;
    int i,clen, len;

    for ( i=0; i<EVP_MAX_IV_LENGTH; i++ )
        iv[i] = i;

    memset(key, 0, EVP_MAX_KEY_LENGTH);
    memcpy(key, passwd, MIN(EVP_MAX_KEY_LENGTH, strlen(passwd)));

    clen = (((strlen(in - 4) + 3) / 4) * 3) + 1;
    base64text = new unsigned char[clen + 1000];
    len = EVP_DecodeBlock(base64text, (unsigned char*)&in[4], strlen(in)-4);

    cipher = EVP_get_cipherbyname(name);
    if ( cipher == NULL )
    {
        OpenSSL_add_all_ciphers();
        cipher = EVP_get_cipherbyname(name);
        if ( cipher == NULL ) return "";
    }

    EVP_DecryptInit(&ctx, cipher, key, NULL);
    ciphertext = new unsigned char[len + EVP_CIPHER_CTX_block_size(&ctx) + 1];

    memcpy(len_str,in,4);
    len_str[4] = '\0';
    sscanf(len_str,"%d", &len);

    EVP_DecryptUpdate(&ctx, ciphertext, &len, base64text, len);
    EVP_DecryptFinal(&ctx, &ciphertext[len], &i);
    len += i;
    ciphertext[len] = '\0';

    ret = (char *)ciphertext;
    delete ciphertext;
    delete base64text;

    return ret;
}
