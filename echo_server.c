#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "ikcp.h"

#define CONV_ID 0x11223344
#define BUF_SIZE 2048

typedef struct UdpCtx {
    int fd;
    struct sockaddr_in peer_addr;
    socklen_t peer_len;
    int has_peer;
} UdpCtx;

static IUINT32 now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (IUINT32)((IUINT32)tv.tv_sec * 1000u + tv.tv_usec / 1000u);
}

static int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user) {
    (void)kcp;
    UdpCtx *ctx = (UdpCtx *)user;
    if (!ctx->has_peer) return 0;

    int n = sendto(ctx->fd, buf, len, 0,
                   (struct sockaddr *)&ctx->peer_addr,
                   ctx->peer_len);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("sendto");
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    if (set_nonblock(fd) < 0) {
        perror("set_nonblock");
        close(fd);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return 1;
    }

    UdpCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.fd = fd;
    ctx.peer_len = sizeof(ctx.peer_addr);

    ikcpcb *kcp = ikcp_create(CONV_ID, &ctx);
    if (!kcp) {
        fprintf(stderr, "ikcp_create failed\n");
        close(fd);
        return 1;
    }
    kcp->output = udp_output;
    ikcp_nodelay(kcp, 1, 10, 2, 1);
    ikcp_wndsize(kcp, 128, 128);

    printf("KCP echo server listening on UDP port %d\n", port);

    char udp_buf[BUF_SIZE];
    char app_buf[BUF_SIZE];

    while(1) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        int n = recvfrom(fd, udp_buf, sizeof(udp_buf), 0,
                         (struct sockaddr *)&from, &from_len);
        
        if (n > 0) {
            ctx.peer_addr = from;
            ctx.peer_len = from_len;
            ctx.has_peer = 1;
            ikcp_input(kcp, udp_buf, n);
            
        }

        while(1) {
            int hr = ikcp_recv(kcp, app_buf, sizeof(app_buf) - 1);
            
            if (hr < 0) break;
            app_buf[hr] = '\0';
            printf("recv: %s\n", app_buf);
            ikcp_send(kcp, app_buf, hr);
        }

        ikcp_update(kcp, now_ms());
        usleep(10000);
    }

    ikcp_release(kcp);
    close(fd);
    return 0;
}
