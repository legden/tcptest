#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#undef NDEBUG
#include <assert.h>

#define SERVER_PORT             5050

//#define debug(...)              fprintf(stdout, __VA_ARGS__)
#define debug(...)

typedef struct client_ctx_s {
    struct sockaddr_in addr;
    int fd;
} client_ctx_t;

void *client_task(void *arg) 
{
    client_ctx_t *client = (client_ctx_t *) arg;
    size_t free_space, pos;
    ssize_t chunk_len;
    uint32_t buff[4] = {0};
    int res;

    pos = 0;
    while ( pos != sizeof(buff) )
    {
        free_space = sizeof(buff) - pos;
        assert(pos < sizeof(buff));

        chunk_len = recv(client->fd, &((uint8_t *) buff)[pos], free_space, 0);
        if ( chunk_len <= 0 ) {
            if ( chunk_len < 0 ) {
                fprintf(stderr, "%s:%u: ERROR: recv failed (errno = %d; pos = %zu)!\n",
                        inet_ntoa(client->addr.sin_addr), 
                        ntohs(client->addr.sin_port),
                        errno, pos);
            }
            else if ( pos && pos < sizeof(buff) ) {
                fprintf(stderr, "%s:%u: ERROR: incomplete data block (pos = %zu)!\n",
                        inet_ntoa(client->addr.sin_addr),
                        ntohs(client->addr.sin_port),
                        pos);
            }
            goto out;
        }

        assert(chunk_len <= free_space);
        pos += chunk_len;

        if ( pos >= 4 && buff[0] != 0xAA55AA55) {
            fprintf(stderr, "%s:%u: ERROR: data corrupted (%08x)!\n", 
                    inet_ntoa(client->addr.sin_addr), 
                    ntohs(client->addr.sin_port),
                    buff[0]);
        }
    }

    fprintf(stdout, "%s:%u: %08x %08x %08x %08x\n",
            inet_ntoa(client->addr.sin_addr),
            ntohs(client->addr.sin_port),
            buff[0], buff[1], buff[2], buff[3]);

out:
    debug("Connection closed\n");
    res = close(client->fd);
    assert(res == 0);
    free(client);
    return NULL;
}

int main(void)
{    
    struct sockaddr_in serveraddr;
    int listenfd, clientfd;
    int optval, res;
    pthread_t thread_id;
    socklen_t len;
    client_ctx_t *client;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if ( listenfd < 0 ) {
        fprintf(stderr, "Can't create socket!\n");
        return -1;
    }

    optval = 1;
    res = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
                     sizeof(int));
    if ( res < 0 ) {
        fprintf(stderr, "setsockopt failed!\n");
        return -1;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVER_PORT);
    res = bind(listenfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
    if ( res < 0 ) {
        fprintf(stderr, "bind failed!\n");
        close(listenfd);
        return -1;  
    }  

    res = listen(listenfd, 5);
    if ( res < 0 ) {
        fprintf(stderr, "listen failed!\n");
        close(listenfd);        
        return -1;  
    }

    fprintf(stdout, "Listening on port %d\n", SERVER_PORT);

    for ( ;; )
    {
        client = malloc(sizeof(client_ctx_t));
        if ( !client ) {
            fprintf(stderr, "malloc failed!\n");
            close(listenfd);            
            return -1;
        }

        len = sizeof(struct sockaddr_in);
        client->fd = accept(listenfd, (struct sockaddr *) &client->addr, &len);
        if ( client->fd < 0 ) {
            fprintf(stderr, "accept failed!\n");            
            close(listenfd);
            return -1;
        }

        debug("Connection from host %s, port %hu.\n",
              inet_ntoa(client->addr.sin_addr),
              ntohs(client->addr.sin_port));

        pthread_create(&thread_id, NULL, client_task, client);
        pthread_detach(thread_id);
    }

    printf("??\n");
    return 0;
}
