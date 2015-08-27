//
// Created by xuan on 8/26/15.
//

#ifndef PROJECT_EVENT_NODE_H
#define PROJECT_EVENT_NODE_H

#include <sys/epoll.h>
#include "libab_thread.h"

typedef struct abio_event_node_s {
    int fd;
    int event_mask;
    abthread_t *context[32];
    struct abio_event_node_s *next;
} abio_event_node_t;

int event_node_init(int epoll_fd);

void event_node_fini(void);

int event_node_set(int fd, enum EPOLL_EVENTS event);

void event_node_raise(uint32_t event, abio_event_node_t *node);
int event_node_clean_thread(abthread_t *);
int event_node_empty(void);

#endif //PROJECT_EVENT_NODE_H
