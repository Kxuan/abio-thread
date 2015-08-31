//
// Created by xuan on 8/26/15.
//

#include <malloc.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "event_node.h"
#include "context.h"

#define EPOLL_EVENT_MAX 100
static struct epoll_event epoll_events[EPOLL_EVENT_MAX];

#define EPOLL_DEFAULT_FLAG (EPOLLET | EPOLLONESHOT)

static abio_event_node_t head, *tail = &head;
static int epoll_fd;

int abevent_init() {
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        perror("epoll_create");
        return 1;
    }
    return 0;
}

void abevent_fini(void) {
    abio_event_node_t *current = head.next, *next;
    head.next = NULL;
    while (current) {
        next = current->next;
        free(current);
        current = next;
    }
}

static int event_schedule(int idx, int op, abio_event_node_t *node) {
    struct epoll_event e = {
            .data.ptr=node,
            .events=(uint32_t) node->event_mask
    };
    if (epoll_ctl(epoll_fd, op, node->fd, &e) != 0)
        return -errno;
    node->context[idx] = abcontext_get_current();
    ab_thread_wait_io();
    return 0;
}

//modify an event node
static int event_node_mod(abio_event_node_t *node) {
    struct epoll_event e = {
            .data.ptr=node,
            .events=(uint32_t) node->event_mask
    };
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, node->fd, &e) != 0)
        return -errno;
    return 0;
}

static int event_node_new(int fd, unsigned event) {
    int rc;
    abio_event_node_t *node = (abio_event_node_t *) malloc(sizeof(abio_event_node_t));
    node->fd = fd;
    node->next = NULL;
    node->event_mask = event | EPOLL_DEFAULT_FLAG;
    tail->next = node;
    tail = node;
    rc = event_schedule(ffs(event) - 1, EPOLL_CTL_ADD, node);
    node->event_mask &= ~event;
    return rc;
}

static int event_node_del(abio_event_node_t *node) {
    struct epoll_event t;
    int fd = node->fd;
    free(node);
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &t) != 0)
        return errno;
    return 0;
}

/*
 * add a fd into event poll.
 *
 * on success, return 0
 * on error, return <0
 * on event raised, return 1
 */
int event_node_set(int fd, enum EPOLL_EVENTS event) {
    int idx = ffs(event);
    if (idx == 0)
        return -EINVAL;
    --idx;
    int rc;

    abio_event_node_t *current_node;
    for (current_node = head.next; current_node != NULL; current_node = current_node->next) {
        if (current_node->fd == fd) {
            if ((current_node->event_mask & event) != 0) {
                //FIXME Queue request
                return -EAGAIN;
            } else {
                current_node->event_mask |= event;
                rc = event_schedule(idx, EPOLL_CTL_MOD, current_node);
                current_node->event_mask &= ~event;
                return rc;
            }
        }
    }
    return event_node_new(fd, event);
}

void event_node_raise(uint32_t event, abio_event_node_t *node) {
    assert(abcontext_is_master());

    int idx;
    uint32_t mask;
    while (event) {
        idx = ffs(event) - 1;
        mask = 1u << idx;
        if (node->event_mask & mask) {
            abcontext_switch_from_master(node->context[idx]);
        }
        event &= ~mask;
    }
    if (node->event_mask != EPOLL_DEFAULT_FLAG) {
        //If event_mask is not empty, re-active it
        event_node_mod(node);
    } else {
        //If event_mask is empty, delete it
        event_node_del(node);
    }
}

int event_node_empty(void) {
    return head.next == NULL;
}

int abevent_clear_context(ucontext_t *context) {
    abio_event_node_t *current_node;
    int is_changed, event_mask, idx, mask, ret;
    for (current_node = head.next; current_node != NULL; current_node = current_node->next) {
        is_changed = 0;
        event_mask = current_node->event_mask;
        while (event_mask) {
            idx = ffs(event_mask) - 1;
            mask = ~(1 << idx);
            if (current_node->context[idx] == context) {
                current_node->event_mask &= mask;
                current_node->context[idx] = NULL;
                is_changed = 1;
            }
            event_mask &= mask;
        }
        if (is_changed) {
            ret = event_node_mod(current_node);
            if (ret) {
                return ret;
            }
        }
    }
    return 0;
}

static void abevent_once() {
    int ready, i;
    if (epoll_fd < 0) {
        fprintf(stderr, "abevent not initialized\n");
        abort();
    }
    ready = epoll_wait(epoll_fd, epoll_events, EPOLL_EVENT_MAX, -1);
    if (ready == 0) {
        //Timeout
        fprintf(stderr, "epoll_wait TIMEOUT(shenme gui)\n");
        abort();
    } else if (ready < 0) {
        switch (errno) {
            case EINTR:
                fprintf(stderr, "interrupted by signal\n");
                return;
            case EBADF:
            case EFAULT:
            case EINVAL:
            default:
                perror("epoll_wait");
                abort();
        }
    } else {
        for (i = 0; i < ready; ++i) {
            event_node_raise(epoll_events[i].events, epoll_events[i].data.ptr);
        }
    }
}

void abevent_loop() {
    assert(abcontext_is_master());

    ab_thread_run();
    while (!event_node_empty()) {
        abevent_once();
        ab_thread_run();
    }
}

