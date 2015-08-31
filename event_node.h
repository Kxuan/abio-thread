//
// Created by xuan on 8/26/15.
//

#ifndef PROJECT_EVENT_NODE_H
#define PROJECT_EVENT_NODE_H

#include <sys/epoll.h>
#include "libab_thread.h"

typedef struct abio_event_node_s {
    int fd;
    unsigned event_mask;
    ucontext_t *context[32];
    struct abio_event_node_s *next;
} abio_event_node_t;

int abevent_init();

void abevent_fini(void);

int event_node_set(int fd, enum EPOLL_EVENTS event);

void event_node_raise(uint32_t event, abio_event_node_t *node);

int abevent_clear_context(ucontext_t *);

int event_node_empty(void);

void abevent_loop();

#endif //PROJECT_EVENT_NODE_H
