//
// Created by xuan on 8/26/15.
//

#include <errno.h>
#include <unistd.h>
#include "event_node.h"


ssize_t ab_read(int fd, void *buf, size_t nbytes) {
    int ret = event_node_set(fd, EPOLLIN);
    switch (ret){
        case -EPERM://The target file fd does not support epoll.
        case 0:
            return read(fd, buf, nbytes);
        default:
            return ret;
    }
}

ssize_t ab_write(int fd, void *buf, size_t nbytes) {
    int ret = event_node_set(fd, EPOLLOUT);
    switch (ret){
        case -EPERM://The target file fd does not support epoll.
        case 0:
            return write(fd, buf, nbytes);
        default:
            return ret;
    }
}