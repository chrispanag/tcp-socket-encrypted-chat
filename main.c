#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "encryptor.h"

#define TCP_BACKLOG 1

#define BUFFER_SIZE 1008

#define DEFAULT_TCP_PORT 3000
#define DEFAULT_HOSTNAME "127.0.0.1"

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

int server(int port, int* sd) {
  if ((*sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  struct sockaddr_in sa;

  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(*sd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
    perror("bind");
    exit(1);
  }

  printf("Waiting for peer...\n");

  /* Listen for incoming connections */
  if (listen(*sd, TCP_BACKLOG) < 0) {
    perror("listen");
    exit(1);
  }

  socklen_t len;
  int newsd;
  char addrstr[INET_ADDRSTRLEN];
  /* Accept an incoming connection */
  len = sizeof(struct sockaddr_in);
  if ((newsd = accept(*sd, (struct sockaddr*)&sa, &len)) < 0) {
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

int e_fd = -1;
struct session_op sess;
unsigned char* key = (unsigned char*)"123456789123456";
unsigned char* iv = (unsigned char*)"123456789123456";

int receive_handler(int fd, char* buffer, int index) {
  int n = read(fd, &buffer[index], (BUFFER_SIZE - (index - 1)) * sizeof(char));
  if (n <= 0) {
    if (n < 0)
      perror("\nread from remote peer failed\n");
    else
      fprintf(stderr, "\nPeer went away\n");
    return -1;
  }

  if (index == 0) {
    printf("\rOther guy: ");
  }

  if ((index + n - 1) % 16 == 0) {
    e_decrypt(e_fd, key, (unsigned char*)&buffer[index], iv,
              (unsigned char*)&buffer[index], n - 1, &sess);
    index += n - 1;

    int charactersToPrint = strlen(buffer);
    if (buffer[charactersToPrint - 1] == '\n' || index > BUFFER_SIZE) {
      if (index > BUFFER_SIZE) {
        charactersToPrint = index - 1;
      }

      int i;
      for (i = 0; i < charactersToPrint - 1; i++) {
        printf("%c", buffer[i]);
      }

      printf("\n");

      if (buffer[charactersToPrint - 1] == '\n') {
        printf("You: ");
      }

      fflush(stdout);
      index = 0;
    }
  }

  return index;
}

// TODO: Check buffer overflow condition
int send_handler(int fd, char* buffer, int index) {
  int n = read(STDIN_FILENO, &buffer[index], BUFFER_SIZE);
  if (buffer[index + n - 1] == '\n') {
    int padding_num = (BUFFER_SIZE - n) % 16;
    memset(&buffer[n], '\0', padding_num);
    e_encrypt(e_fd, key, (unsigned char*)&buffer[index], iv,
              (unsigned char*)&buffer[index], n + padding_num, &sess);
    index += n + padding_num;
    printf("You: ");
    buffer[index + 1] = '\0';
    if (insist_write(fd, buffer, index + 1) != index + 1) {
      perror("write");
      exit(1);
    }

    index = 0;

    fflush(stdout);
  }

  return index;
}

char readBuffer[BUFFER_SIZE];
char receiveBuffer[BUFFER_SIZE];
int socketFd;
int serverFd = -1;

int main(int argc, char** argv) {
  if (argc < 2 || argc > 4) {
    printf("Usage: <server|client> <shared_key> <port> <hostname>\n");
    exit(1);
  }

  int listen_port = DEFAULT_TCP_PORT;
  char* hostname = DEFAULT_HOSTNAME;

  if (argc > 2) {
    listen_port = atoi(argv[2]);
  }

  if (argc > 3) {
    hostname = argv[3];
  }

  if (strcmp(argv[1], "server") == 0) {
    socketFd = server(listen_port, &serverFd);
  } else {
    socketFd = client(hostname, listen_port);
  }

  e_fd = openEncryptor();
  createEncryptionSession(e_fd, key, &sess);

  fd_set fdset;
  int readCount = 0, receiveCount = 0;

  printf("You: ");
  fflush(stdout);
  for (;;) {
    FD_ZERO(&fdset);
    FD_SET(STDIN_FILENO, &fdset);
    FD_SET(socketFd, &fdset);

    if (select(socketFd + 1, &fdset, NULL, NULL, NULL) < 0) {
      printf("error select\n");
      exit(1);
    }

    if (FD_ISSET(STDIN_FILENO, &fdset)) {
      readCount = send_handler(socketFd, readBuffer, readCount);
      if (readCount < 0) {
        exit(1);
      }
    }

    if (FD_ISSET(socketFd, &fdset)) {
      receiveCount = receive_handler(socketFd, receiveBuffer, receiveCount);
      if (receiveCount < 0) {
        exit(1);
      }
    }
  }

  exit(0);
}