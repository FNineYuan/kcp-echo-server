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
    struct sockaddr_in server_addr;
    socklen_t server_len;
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
    int n = sendto(ctx->fd, buf, len, 0,
                   (struct sockaddr *)&ctx->server_addr,
                   ctx->server_len);
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("sendto");
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <server_ip> <server_port> <message>\n", argv[0]);
        return 1;
    }

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

    UdpCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.fd = fd;
    ctx.server_len = sizeof(ctx.server_addr);
    ctx.server_addr.sin_family = AF_INET;
    ctx.server_addr.sin_port = htons((uint16_t)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &ctx.server_addr.sin_addr) != 1) {
        fprintf(stderr, "invalid server ip\n");
        close(fd);
        return 1;
    }

    ikcpcb *kcp = ikcp_create(CONV_ID, &ctx);
    if (!kcp) {
        fprintf(stderr, "ikcp_create failed\n");
        close(fd);
        return 1;
    }
    kcp->output = udp_output;
    ikcp_nodelay(kcp, 1, 10, 2, 1);
    ikcp_wndsize(kcp, 128, 128);

    const char *msg = argv[3];
    ikcp_send(kcp, msg, (int)strlen(msg));

    char udp_buf[BUF_SIZE];
    char app_buf[BUF_SIZE];
    IUINT32 start = now_ms();

    for (;;) {
        int n = recvfrom(fd, udp_buf, sizeof(udp_buf), 0, NULL, NULL);
        if (n > 0) {
            ikcp_input(kcp, udp_buf, n);
        }

        int hr = ikcp_recv(kcp, app_buf, sizeof(app_buf) - 1);
        if (hr >= 0) {
            app_buf[hr] = '\0';
            printf("echo: %s\n", app_buf);
            break;
        }

        ikcp_update(kcp, now_ms());

        if (now_ms() - start > 5000) {
            fprintf(stderr, "timeout waiting echo\n");
            break;
        }
        usleep(1000);
    }

    ikcp_release(kcp);
    close(fd);
    return 0;
}
