//
// Created by xuan on 8/26/15.
//

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "event_node.h"

#if EAGAIN == EWOULDBLOCK
#define EAGAIN_OR_EWOULDBLOCK EAGAIN
#else
#define EAGAIN_OR_EWOULDBLOCK EAGAIN: case EWOULDBLOCK
#endif

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