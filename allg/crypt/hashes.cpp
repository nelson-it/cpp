#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include <message/message.h>
#include "openssl/evp.h"

std::string digests_base64(char *name, const char *in)
{
    const EVP_MD *m;
    EVP_MD_CTX ctx;
    unsigned char hash[EVP_MAX_MD_SIZE];
    char hash64[(((EVP_MAX_MD_SIZE + 2) / 3) * 4) + 1];
    unsigned int hashlen;

    OpenSSL_add_all_digests();
    m = EVP_get_digestbyname(name);
    if ( m == NULL )
    {
        Message msg("simple_digest");
        msg.perror(1,"Digest <%s> unbekannt", name);
        return "";
    }

    EVP_DigestInit(&ctx, m);
    EVP_DigestUpdate(&ctx, in, strlen(in));
    EVP_DigestFinal(&ctx, (unsigned char*)&hash, &hashlen);

    hash64[EVP_EncodeBlock((unsigned char*)&hash64, (unsigned char*)&hash, hashlen)] = '\0';

    return hash64;
}
