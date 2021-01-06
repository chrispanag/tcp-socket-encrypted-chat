#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <crypto/cryptodev.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define DEFAULT_TCP_PORT 3000
#define DEFAULT_HOSTNAME "127.0.0.1"
#define BUFFER_SIZE 1000
#define TCP_BACKLOG 1
#define BLOCK_SIZE 16
#define KEY_SIZE 16 /* AES128 */

cryptodev_verbosity = 10000;

struct chat_config {
    int file_descriptor;
    int decryptor;
    unsigned char *iv,
        *key;
    struct session_op* sess;
};

typedef struct chat_config chat_config;

int openEncryptor() {
    int fd = open("/dev/crypto", O_RDWR);
    if (fd < 0) {
        perror("open(/dev/crypto)");
        return 1;
    }

    return fd;
}

int createEncryptionSession(int cfd, unsigned char* key, struct session_op* sess) {
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

int encrypt(int cfd, unsigned char* key, unsigned char* data, unsigned char* iv, unsigned char* output, unsigned short operation, size_t n, struct session_op* sess) {
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

int client(char* hostname, int port) {
    int sd;
    /* Create TCP/IP socket, used as main chat channel */
    if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    struct hostent* hp;
    struct sockaddr_in sa;

    /* Look up remote hostname on DNS */
    if (!(hp = gethostbyname(hostname))) {
        printf("DNS lookup failed for host %s\n", hostname);
        exit(1);
    }

    /* Connect to remote TCP port */
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    memcpy(&sa.sin_addr.s_addr, hp->h_addr, sizeof(struct in_addr));
    if (connect(sd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("connect");
        exit(1);
    }

    return sd;
}

int server(int port) {
    int sd;

    if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in sa;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("bind");
        exit(1);
    }

    printf("Waiting for peer...\n");

    /* Listen for incoming connections */
    if (listen(sd, TCP_BACKLOG) < 0) {
        perror("listen");
        exit(1);
    }

    socklen_t len;
    int newsd, n;
    char addrstr[INET_ADDRSTRLEN], buf[2];
    /* Accept an incoming connection */
    len = sizeof(struct sockaddr_in);
    if ((newsd = accept(sd, (struct sockaddr*)&sa, &len)) < 0) {
        perror("accept");
        exit(1);
    }
    if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr))) {
        perror("could not format IP address");
        exit(1);
    }
    printf("Incoming connection from %s:%d\n", addrstr, ntohs(sa.sin_port));

    return newsd;
}

ssize_t insist_write(int fd, const void* buf, size_t cnt) {
    ssize_t ret;
    size_t orig_cnt = cnt;

    while (cnt > 0) {
        ret = write(fd, buf, cnt);
        if (ret < 0)
            return ret;
        buf += ret;
        cnt -= ret;
    }

    return orig_cnt;
}

void exitChat(int d) {
    if (shutdown(d, SHUT_WR) < 0) {
        perror("shutdown");
        exit(1);
    }

    printf("Exiting...\n");
}

void* handleReceive(void* vargp) {
    chat_config config = *(chat_config*)vargp;
    int n, i;
    unsigned char buf[BUFFER_SIZE];
    for (;;) {
        n = read(config.file_descriptor, buf, sizeof(buf));

        if (n <= 0) {
            if (n < 0)
                perror("\nread from remote peer failed\n");
            else
                fprintf(stderr, "\nPeer went away\n");
            break;
        }

        unsigned char* decrypted = (char*) malloc(BUFFER_SIZE * sizeof(unsigned char));
        memset(decrypted, '\0', BUFFER_SIZE);
        if (encrypt(config.decryptor, config.key, buf, config.iv, decrypted, COP_DECRYPT, n - 1, config.sess) != 0) {
            printf("\nDecryption issue!\n");
            exit(1);
        }
        
        printf("\rOther guy: ");
        for (i = 0; i < n; i++) {
            printf("%c", decrypted[i]);
        }
        printf("You: ");

        // Delete last line, print the message and then print the prompt
        fflush(stdout);
    }

    return NULL;
}

void handleSend(chat_config config) {
    unsigned char readbuf[BUFFER_SIZE];
    int n;

    for (;;) {
        fgets(readbuf, BUFFER_SIZE - 1, stdin);
        if (readbuf[0] == '\n') {
            continue;
        }
        if (strcmp(readbuf, "/exit\n") == 0) {
            exitChat(config.file_descriptor);
            break;
        }
        printf("You: ");
        n = strlen(readbuf);
        // /* Be careful with buffer overruns, ensure NUL-termination */

        unsigned char* encrypted = (char*) malloc(BUFFER_SIZE * sizeof(unsigned char));
        if (encrypt(config.decryptor, config.key, readbuf, config.iv, encrypted, COP_ENCRYPT, n, config.sess) != 0) {
            printf("\nEncryption issue!\n");
            exit(1);
        }

        if (insist_write(config.file_descriptor, encrypted, n + 1) != n + 1) {
            perror("write");
            exit(1);
        }

        readbuf[0] = '\0';
        fflush(stdout);
    }
}

int file_descriptor = -1;
pthread_t thread_id;

void handle_sigint(int sig) {
    if (file_descriptor != -1) {
        pthread_kill(thread_id, SIGKILL);
        if (close(file_descriptor) < 0)
            perror("close");
    }
    exit(0);
}

int main(int argc, char** argv) {
    int listen_port = DEFAULT_TCP_PORT;
    char* hostname = DEFAULT_HOSTNAME;

    if (argc < 3 || argc > 5) {
        printf("Usage: <server|client> <shared_key> <port> <hostname>\n");
        exit(1);
    }

    char* key_and_iv = argv[2];
    char iv[16];
    memcpy(iv, &key_and_iv[16], 16);
    char key[16];
    memcpy(key, key_and_iv, 16);
    if (argc > 3) {
        listen_port = atoi(argv[3]);
    }
    if (argc == 5) {
        hostname = argv[4];
    }

    signal(SIGINT, handle_sigint);
    if (strcmp(argv[1], "server") == 0) {
        file_descriptor = server(listen_port);
    } else {
        file_descriptor = client(hostname, listen_port);
    }

    int encryptorFile = openEncryptor();

    struct session_op sess;
    createEncryptionSession(encryptorFile, key, &sess);

    chat_config config;
    config.file_descriptor = file_descriptor;
    config.iv = iv;
    config.key = key;
    config.decryptor = encryptorFile;
    config.sess = &sess;

    printf("You: ");
    fflush(stdout);

    pthread_create(&thread_id, NULL, handleReceive, (void*)&config);
    handleSend(config);

    // if (ioctl(encryptorFile, CIOCFSESSION, &sess.ses)) {
    //     perror("ioctl(CIOCFSESSION)");
    //     return 1;
    // }

    if (close(file_descriptor) < 0)
        perror("close");

    return 1;
}