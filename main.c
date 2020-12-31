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

#define DEFAULT_TCP_PORT 3000
#define DEFAULT_HOSTNAME "127.0.0.1"
#define BUFFER_SIZE 1000
#define TCP_BACKLOG 1

void *myThreadFun(void *vargp) {
  int *d = (int *)vargp;
  return NULL;
}

int client(char *hostname, int port) {
  int sd;
  /* Create TCP/IP socket, used as main chat channel */
  if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  struct hostent *hp;
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
  if (connect(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
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
  if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
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
  if ((newsd = accept(sd, (struct sockaddr *)&sa, &len)) < 0) {
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

ssize_t insist_write(int fd, const void *buf, size_t cnt) {
  ssize_t ret;
  size_t orig_cnt = cnt;

  while (cnt > 0) {
    ret = write(fd, buf, cnt);
    if (ret < 0) return ret;
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

void *handleReceive(void *vargp) {
  int d = *(int *)vargp;
  int n;
  char buf[BUFFER_SIZE];
  for (;;) {
    n = read(d, buf, sizeof(buf));

    if (n <= 0) {
      if (n < 0)
        perror("\nread from remote peer failed\n");
      else
        fprintf(stderr, "\nPeer went away\n");
      break;
    }

    // Delete last line, print the message and then print the prompt
    printf("\rOther guy: %sYou: ", buf);
    fflush(stdout);
  }

  return NULL;
}

void handleSend(int d) {
  char readbuf[BUFFER_SIZE];
  int n;

  for (;;) {
    fgets(readbuf, BUFFER_SIZE - 1, stdin);
    if (readbuf[0] == '\n') {
      continue;
    }
    if (strcmp(readbuf, "/exit\n") == 0) {
      exitChat(d);
      break;
    }
    printf("You: ");
    n = strlen(readbuf);
    // /* Be careful with buffer overruns, ensure NUL-termination */

    if (insist_write(d, readbuf, n + 1) != n + 1) {
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
    if (close(file_descriptor) < 0) perror("close");
  }
  exit(0);
}

int main(int argc, char **argv) {
  int listen_port = DEFAULT_TCP_PORT;
  char *hostname = DEFAULT_HOSTNAME;
  if (argc < 2 || argc > 4) {
    printf("Usage: <server|client> <port> <hostname>\n");
    exit(1);
  }

  if (argc > 2) {
    listen_port = atoi(argv[2]);
  }
  if (argc == 4) {
    hostname = argv[3];
  }

  signal(SIGINT, handle_sigint);

  if (strcmp(argv[1], "server") == 0) {
    file_descriptor = server(listen_port);
  } else {
    file_descriptor = client(hostname, listen_port);
  }

  printf("You: ");
  fflush(stdout);

  pthread_create(&thread_id, NULL, handleReceive, (void *)&file_descriptor);
  handleSend(file_descriptor);

  if (close(file_descriptor) < 0) perror("close");

  return 1;
}