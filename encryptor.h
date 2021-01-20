#ifndef L_ENCRYPTOR_H
#define L_ENCRYPTOR_H

#include <stdlib.h>
#include <crypto/cryptodev.h>
#endif

int openEncryptor();
int createEncryptionSession(int cfd, unsigned char* key,
                            struct session_op* sess);
int e_encrypt(int cfd, unsigned char* key, unsigned char* data, unsigned char* iv,
            unsigned char* output, size_t n,
            struct session_op* sess);
int e_decrypt(int cfd, unsigned char* key, unsigned char* data, unsigned char* iv,
            unsigned char* output, size_t n,
            struct session_op* sess);
