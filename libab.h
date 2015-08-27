//
// Created by xuan on 8/26/15.
//

#ifndef PROJECT_LIBAB_H
#define PROJECT_LIBAB_H

#include <sys/types.h>

ssize_t ab_read(int fd, void *buf, size_t nbytes);
ssize_t ab_write(int fd, void *buf, size_t nbytes);

#endif //PROJECT_LIBAB_H
