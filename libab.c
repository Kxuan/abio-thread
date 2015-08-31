//
// Created by xuan on 8/26/15.
//
#define _GNU_SOURCE

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/socket.h>
#include "event_node.h"

#if EAGAIN == EWOULDBLOCK
#define EAGAIN_OR_EWOULDBLOCK EAGAIN
#else
#define EAGAIN_OR_EWOULDBLOCK EAGAIN: case EWOULDBLOCK
#endif


int ab_open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    flags |= O_NONBLOCK | O_NDELAY;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return open(pathname, flags, mode);
}


int ab_creat(const char *pathname, mode_t mode) {
    return ab_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}


int ab_manage(int fd) {
    int fl = fcntl(fd, F_GETFL);
    if (fl == -1) {
        return fl;
    }
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}


int ab_close(int fd) {
    return close(fd);
}


int ab_socket(int domain, int type, int protocol) {
    return socket(domain, type | O_NONBLOCK, protocol);
}

int ab_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return bind(sockfd, addr, addrlen);
}

int ab_listen(int sockfd, int backlog) {
    return listen(sockfd, backlog);
}


int ab_shutdown(int sockfd,int how) {
    return shutdown(sockfd,how);
}

int ab_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
    int rc;
    rc = accept4(sockfd, addr, addrlen, flags | SOCK_NONBLOCK);
    if (rc >= 0) {
        return rc;
    } else {
        switch (errno) {
            case EAGAIN_OR_EWOULDBLOCK:
                rc = event_node_set(sockfd, EPOLLIN);
                switch (rc){
                    case -EAGAIN:
                        fprintf(stderr,"accept conflict on socket %d\n",sockfd);
                        abort();
                    case 0:
                        return accept4(sockfd, addr, addrlen, flags | SOCK_NONBLOCK);
                    default:
                        return -1;
                }
                break;
            default:
                return rc;
        }
    }
}


int ab_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return ab_accept4(sockfd, addr, addrlen, 0);
}


ssize_t ab_read(int fd, void *buf, size_t nbytes) {
    ssize_t retsize;
    int rc;
    retsize = read(fd, buf, nbytes);
    if (retsize >= 0) {
        return retsize;
    } else {
        switch (errno) {
            case EAGAIN_OR_EWOULDBLOCK:
                rc = event_node_set(fd, EPOLLIN);
                switch (rc) {
                    case -EPERM:
                        fprintf(stderr, "File %d does not support epoll\n", fd);
                        abort();
                    case -EAGAIN:
                        fprintf(stderr, "read conflict on file %d\n", fd);
                        abort();
                    case 0:
                        return read(fd, buf, nbytes);
                    default:
                        return -1;
                }
                break;
            default:
                return retsize;
        }
    }
}

ssize_t ab_write(int fd, void *buf, size_t nbytes) {
    ssize_t retsize;
    int rc;
    retsize = write(fd, buf, nbytes);
    if (retsize >= 0) {
        return retsize;
    } else {
        switch (errno) {
            case EAGAIN_OR_EWOULDBLOCK:
                rc = event_node_set(fd, EPOLLOUT);
                switch (rc) {
                    case -EPERM:
                        fprintf(stderr, "File %d does not support epoll\n", fd);
                        abort();
                    case -EAGAIN:
                        fprintf(stderr, "write conflict on file %d\n", fd);
                        abort();
                    case 0:
                        return write(fd, buf, nbytes);
                    default:
                        return -1;
                }
                break;
            default:
                return retsize;
        }
    }
}