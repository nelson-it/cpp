#ifndef mne_sha1
#define mne_sha1

#include <stdint.h>
#include <string>

#define SHA1HashSize 20

class Sha1
{
public:
    enum
    {
        shaSuccess = 0,
        shaNull,            /* Null pointer parameter */
        shaInputTooLong,    /* input data too long */
        shaStateError       /* called Input after Result */
    };

private:
    uint32_t Intermediate_Hash[SHA1HashSize/4]; /* Message Digest  */

    uint32_t Length_Low;            /* Message length in bits      */
    uint32_t Length_High;           /* Message length in bits      */

                               /* Index into message block array   */
    int_least16_t Message_Block_Index;
    uint8_t Message_Block[64];      /* 512-bit message blocks      */

    int Computed;               /* Is the digest computed?         */
    int Corrupted;             /* Is the message digest corrupted? */

    void padmessage();
    void processmessageblock();

public:
    Sha1() { reset(); };
    ~Sha1() {};

    int reset();
    int set(const uint8_t *in, unsigned int size);
    int get(uint8_t *Message_Digest);

};


#endif /* mne_sha1 */
