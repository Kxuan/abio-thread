//
// Created by xuan on 8/26/15.
//

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "libab_thread.h"
#include "libab.h"

static int do_echo(int fdin, int fdout) {
    char buff[1024];
    ssize_t retsize;

    while ((retsize = ab_read(fdin, buff, 1024)) > 0) {
        ab_write(fdout, buff, (size_t) retsize);
        if (strcmp(buff, "exit\n") == 0)
            break;
    }
    if (retsize == -1)
        return errno;
    else
        return 0;
}

static void *thread_console(void *arg) {
    if (ab_manage(STDIN_FILENO) == -1 || ab_manage(STDOUT_FILENO) == -1)
        return (void *) (intptr_t) errno;

    do_echo(STDIN_FILENO, STDOUT_FILENO);
    exit(0);
}

static void *thread_tcp_handler(void *v_fd) {
    int fd = (int) (intptr_t) v_fd,
            rc;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    char addr_text[20] = {0};

    rc = getpeername(fd, (struct sockaddr *) &addr, &addr_len);
    inet_ntop(AF_INET, &addr.sin_addr, addr_text, sizeof(addr_text));

    printf("[%s]Connected\n", addr_text);
    if (rc < 0) {
        fprintf(stderr, "Fail to getpeername on %d\n", fd);
        return (void *) (intptr_t) errno;
    }

    rc = do_echo(fd, fd);

    ab_shutdown(fd, SHUT_RDWR);
    ab_close(fd);
    fprintf(stdout, "[%s]Disconnected\n", addr_text);
    return (void *) (intptr_t) rc;
}

static void *thread_tcp(void *arg) {
    int listener, child;
    int rc;
    struct sockaddr_in addr;
    socklen_t addr_len;

    listener = ab_socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("ab_socket");
        goto fail_socket;
    }
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1802);

    rc = ab_bind(listener, (const struct sockaddr *) &addr, sizeof(addr));
    if (rc < 0) {
        perror("ab_bind");
        goto fail_bind;
    }
    rc = ab_listen(listener, 50);
    if (rc < 0) {
        perror("ab_listen");
        goto fail_listen;
    }
    while ((child = ab_accept(listener, (struct sockaddr *) &addr, &addr_len)) >= 0) {
        ab_thread_create(thread_tcp_handler, (void *) (intptr_t) child);
    }

    fail_listen:
    fail_bind:
    ab_close(listener);
    fail_socket:
    fprintf(stderr, "thread_tcp() exit\n");
    return arg;
}

static void *thread_copy(void *arg) {
    char buff[1024];
    int fdin, fdout;
    uint32_t copy_count = 0;
    fdin = open("/dev/random", O_RDONLY | O_NONBLOCK);
    if (fdin < 0) {
        perror("/dev/random.");
        goto out_open_fdin;
    }
    fdout = open("/dev/zero", O_WRONLY | O_NONBLOCK);
    if (fdout < 0) {
        perror("/dev/zero");
        goto out_open_fdout;
    }

    ssize_t retsize;
    while ((retsize = ab_read(fdin, buff, 1024)) > 0) {
        ab_write(fdout, buff, (size_t) retsize);
        copy_count += retsize;
        fprintf(stderr, "Copied %u bytes\n", copy_count);
    }

    close(fdout);
    out_open_fdout:
    close(fdin);
    out_open_fdin:
    if (retsize) {
        fprintf(stderr, "thread_copy exited (Error:%s)\n", strerror(-(int) retsize));
    }
    return arg;
}

int test_main() {
    ab_thread_create(thread_copy, (void *) 2);
    ab_thread_create(thread_console, (void *) 1);
    ab_thread_create(thread_tcp, (void *) 3);
    return 0;
}