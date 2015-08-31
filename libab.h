//
// Created by xuan on 8/26/15.
//

#ifndef PROJECT_LIBAB_H
#define PROJECT_LIBAB_H

#include <sys/types.h>

int ab_open(const char *pathname, int flags, ...);

int ab_creat(const char *pathname, mode_t mode);

int ab_manage(int fd);

int ab_close(int fd);

int ab_socket(int domain, int type, int protocol);

int ab_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int ab_listen(int sockfd, int backlog);

int ab_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
int ab_shutdown(int sockfd,int how);

int ab_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

ssize_t ab_read(int fd, void *buf, size_t nbytes);

ssize_t ab_write(int fd, void *buf, size_t nbytes);

#endif //PROJECT_LIBAB_H

