#ifndef cipher_mne
#define cipher_mne

#include <string>

std::string encode_cipher_base64(char *name, const char *in, const char *passwd);
std::string decode_cipher_base64(char *name, const char *in, const char *passwd);


#endif
