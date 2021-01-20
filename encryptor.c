#ifndef L_ENCRYPTOR
#define L_ENCRYPTOR

#include <crypto/cryptodev.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#endif

int openEncryptor() {
  int fd = open("/dev/crypto", O_RDWR);
  if (fd < 0) {
    perror("open(/dev/crypto)");
    return 1;
  }

  return fd;
}

int createEncryptionSession(int cfd, unsigned char* key,
                            struct session_op* sess) {
  memset(sess, 0, sizeof(*sess));

  sess->cipher = CRYPTO_AES_CTR;
  sess->keylen = KEY_SIZE;
  sess->key = key;

  if (ioctl(cfd, CIOCGSESSION, sess)) {
    perror("ioctl(CIOCGSESSION)");
    return 1;
  }

  return 0;
}

int encrypt_fun(int cfd, unsigned char* key, unsigned char* data,
                unsigned char* iv, unsigned char* output,
                unsigned short operation, size_t n, struct session_op* sess) {
  struct crypt_op cryp;

  memset(&cryp, 0, sizeof(cryp));

  cryp.ses = sess->ses;
  cryp.len = n;
  cryp.src = data;
  cryp.dst = output;
  cryp.iv = iv;
  cryp.op = operation;

  if (ioctl(cfd, CIOCCRYPT, &cryp)) {
    perror("ioctl(CIOCCRYPT)");
    return 1;
  }

  return 0;
}

int e_encrypt(int cfd, unsigned char* key, unsigned char* data, unsigned char* iv,
            unsigned char* output, size_t n, struct session_op* sess) {
  return encrypt_fun(cfd, key, data, iv, output, COP_ENCRYPT, n, sess);
}

int e_decrypt(int cfd, unsigned char* key, unsigned char* data, unsigned char* iv,
            unsigned char* output, size_t n, struct session_op* sess) {
  return encrypt_fun(cfd, key, data, iv, output, COP_DECRYPT, n, sess);
}