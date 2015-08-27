//
// Created by xuan on 8/26/15.
//

#include <unistd.h>
#include <stdlib.h>
#include "event_node.h"
#include "context.h"


ssize_t ab_read(int fd, void *buf, size_t nbytes) {
    int ret = event_node_set(fd, EPOLLIN);
    switch (ret) {
        case 0:
            abio_loop();
            abort();
        case 1:
            return read(fd, buf, nbytes);
        default:
            return ret;
    }
}

ssize_t ab_write(int fd, void *buf, size_t nbytes) {
    int ret = event_node_set(fd, EPOLLOUT);
    switch (ret) {
        case 0:
            abio_loop();
            abort();
        case 1:
            return write(fd, buf, nbytes);
        default:
            return ret;
    }
}