#include <stdio.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#define DEFAULT_TCP_PORT 3000

const int TCP_BACKLOG = 5;

void handleConnection(int newsd)
{
    int n;
    char buf[100];
    for (;;)
    {
        n = read(newsd, buf, sizeof(buf));
        if (n <= 0)
        {
            if (n < 0)
                perror("read from remote peer failed");
            else
                fprintf(stderr, "Peer went away\n");
            break;
        }
        printf("%s\n", buf);
    }
}

void server(int port)
{
    int sd;

    if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }
    fprintf(stderr, "Created TCP socket\n");

    struct sockaddr_in sa;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
        perror("bind");
        exit(1);
    }
    fprintf(stderr, "Bound TCP socket to port %d\n", port);

    /* Listen for incoming connections */
    if (listen(sd, TCP_BACKLOG) < 0)
    {
        perror("listen");
        exit(1);
    }

    socklen_t len;
    int newsd, n;
    char addrstr[INET_ADDRSTRLEN], buf[2];
    for (;;)
    {
        fprintf(stderr, "Waiting for an incoming connection...\n");

        /* Accept an incoming connection */
        len = sizeof(struct sockaddr_in);
        if ((newsd = accept(sd, (struct sockaddr *)&sa, &len)) < 0)
        {
            perror("accept");
            exit(1);
        }
        if (!inet_ntop(AF_INET, &sa.sin_addr, addrstr, sizeof(addrstr)))
        {
            perror("could not format IP address");
            exit(1);
        }
        fprintf(stderr, "Incoming connection from %s:%d\n",
                addrstr, ntohs(sa.sin_port));

        handleConnection(newsd);

        if (close(newsd) < 0)
            perror("close");
    }
}

int main(int argc, char **argv)
{
    int port = DEFAULT_TCP_PORT;
    if (argc > 1)
    {
        port = atoi(argv[1]);
    }

    server(port);

    return 0;
}
