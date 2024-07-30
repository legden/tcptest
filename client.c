#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>    
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <getopt.h>
#include <stdbool.h>

#define SERVER_ADDR             "127.0.0.1"
#define SERVER_PORT             5050

typedef struct connection_ctx_s {
    struct sockaddr_in *serveraddr;    
    pthread_t thread_id;
    size_t sent_packs;
} connection_ctx_t;

static struct option long_opts[] = {
    {"threads",   required_argument, 0, 't'},
    {"count",     required_argument, 0, 'c'},    
    {"delay",     required_argument, 0, 'd'},
    {"fragments", no_argument,       0, 'f'},
    {"port",      no_argument,       0, 'p'},    
    {"help",      no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

char server_name[256] = SERVER_ADDR;
unsigned short server_port = SERVER_PORT;
unsigned int n_threads = 1;
unsigned int count = 1;
unsigned int delay = 1000;
bool frags = 0;

static size_t send_ex(int fd, const uint8_t *buff, size_t len, bool by_frags)
{
    if ( by_frags )
    {
        size_t chunk_len, pos;
        size_t res;

        for ( pos = 0; pos < len;  )
        {
            chunk_len = (size_t) random();
            chunk_len %= (len - pos);
            chunk_len++;

            res = send(fd, (const char *) &buff[pos], chunk_len, 0);
            if ( res != chunk_len) {
                return (size_t) -1;
            }

            pos += chunk_len;
        }

        return len;
    }

    return send(fd, buff, len, 0);
}

static int get_server_addr(struct sockaddr_in *serveraddr, const char *hint)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int res;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    res = getaddrinfo(hint, NULL, &hints, &result);
    if ( !res ) {
        memset(serveraddr, 0, sizeof(struct sockaddr_in));
        serveraddr->sin_family = AF_INET;
        serveraddr->sin_port = htons(server_port);

        for ( rp = result; rp != NULL; rp = rp->ai_next )
        {
            switch (rp->ai_family)
            {
            case AF_INET:
                serveraddr->sin_addr = ((struct sockaddr_in *) rp->ai_addr)->sin_addr;
                freeaddrinfo(result);
                return 0;

            default:
                break;
            }
        }
        freeaddrinfo(result);
    }

    return -1;
}

static void msleep(int tms)
{
    struct timeval tv = {
        .tv_sec = tms / 1000,
        .tv_usec = (tms % 1000) * 1000
    };

    select(0, NULL, NULL, NULL, &tv);
}

static void *connection_task(void *arg) 
{   
    connection_ctx_t *ctx = (connection_ctx_t *) arg;
    uint32_t buff[4] = {0xAA55AA55, 0x12345678, 0x12345678, 0x12345678};
    int res, fd, i;

    for ( i = 0; i < count; i++ )
    {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if ( fd < 0 ) {
            fprintf(stderr, "Can't create socket!\n");
            break;
        }

        res = connect(fd, (struct sockaddr *) ctx->serveraddr, sizeof(struct sockaddr_in));
        if ( res < 0 ) {
            fprintf(stderr, "Connect failed!\n");                    
            close(fd);
            break;
        }

        res = send_ex(fd, (const char *) buff, sizeof(buff), frags);
        if ( res != sizeof(buff) ) {
            fprintf(stderr, "Send failed!\n");
            close(fd);
            break;
        }

        ctx->sent_packs++;

        res = close(fd);
        if ( res < 0 ) {
            fprintf(stderr, "CLI: Close Failed!!\n");
        }

        msleep(delay);
    }

    return NULL;
}

static void usage(const char *name)
{
	printf("Usage:\n");
	printf("  %s [options] [server]\n", name);
    printf("\n");    
	printf("Options:\n");
	printf("  %-16s %s\n", "-t, --threads", "number of parallel connections");
	printf("  %-16s %s\n", "-c, --count", "packets to send per connection");
	printf("  %-16s %s\n", "-d, --delay", "delay between packets (msec)");    
	printf("  %-16s %s\n", "-f, --fragments", "force packets fragmentation");
	printf("  %-16s %s\n", "-p, --port", "server port to connect");
	exit(-1);
}

int main(int argc, char *argv[])
{    
    struct sockaddr_in serveraddr;
    int optval, res, c;
    connection_ctx_t *ctxs;
    size_t sz;

    while ( 1 )
    {
		c = getopt_long(argc, argv, "t:c:d:fp:h", long_opts, NULL);
		if ( c == -1 )
			break;

        switch ( c )
        {
        case 't':
            sscanf(optarg, "%d", &n_threads);
            break;
        
        case 'c':
            sscanf(optarg, "%d", &count);
            break;

        case 'd':
            sscanf(optarg, "%d", &delay);
            break;

        case 'f':
            frags = 1;
            break;

        case 'p':
            sscanf(optarg, "%hd", &server_port);
            break;

        default:
            usage(argv[0]);
        }
    }

    if (optind < argc) {
		sscanf(argv[optind++], "%255s", server_name);
    }

    // fprintf(stdout, "server_name = %s\n", server_name);
    // fprintf(stdout, "server_port = %u\n", server_port);
    // fprintf(stdout, "n_threads = %u\n", n_threads);
    // fprintf(stdout, "count = %u\n", count);
    // fprintf(stdout, "delay = %u\n", delay);
    // fprintf(stdout, "fragmentate = %u\n", frags);

    sz = sizeof(connection_ctx_t) * n_threads;
    if ( !(ctxs = malloc(sz)) ) {
        fprintf(stderr, "Can't allocate %zu bytes!\n", sz);
        return -1;
    }

    res = get_server_addr(&serveraddr, server_name);
    if ( res < 0 ) {
        fprintf(stderr, "Can't find %s!\n", server_name);
        free(ctxs);
        return -1;
    }
    
    fprintf(stderr, "Connecting to %s:%d\n", inet_ntoa(serveraddr.sin_addr), 
            ntohs(serveraddr.sin_port));

    for ( c = 0; c < n_threads; c++ )
    {
        ctxs[c].serveraddr = &serveraddr;
        ctxs[c].sent_packs = 0;
        res = pthread_create(&ctxs[c].thread_id, NULL, connection_task, &ctxs[c]);
        if ( res != 0 ) {
            fprintf(stderr, "Can't create thread %d!\n", c);
            break;
        }
    }

    for (sz = 0; c > 0; c--)
    {
        pthread_join(ctxs[c - 1].thread_id, NULL);
        sz += ctxs[c - 1].sent_packs;
    }

    fprintf(stdout, "Total packets sent: %zu\n", sz);
    free(ctxs);

    return 0;
}
