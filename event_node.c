//
// Created by xuan on 8/26/15.
//

#include <malloc.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "event_node.h"
#include "context.h"

static abio_event_node_t head, *tail = &head;
static int epoll_fd;

int event_node_init(int v_epoll_fd) {
    epoll_fd = v_epoll_fd;
    return 0;
}

void event_node_fini(void) {
    abio_event_node_t *current = head.next, *next;
    head.next = NULL;
    while (current) {
        next = current->next;
        free(current);
        current = next;
    }
}

static int event_node_jmp_ctl(int idx, int op, abio_event_node_t *node) {
    struct epoll_event e = {
            .data.ptr=node,
            .events=(uint32_t) node->event_mask
    };
    if (epoll_ctl(epoll_fd, op, node->fd, &e) != 0)
        return -errno;
    node->context[idx] = ab_thread_current();
    if (abio_context_swap_to_global(&node->context[idx]->context) != 0)
        return -errno;
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

static int event_node_new(int fd, int event) {
    //TODO Optimise.
    abio_event_node_t *node = (abio_event_node_t *) malloc(sizeof(abio_event_node_t));
    node->fd = fd;
    node->next = NULL;
    node->event_mask = event | EPOLLONESHOT;
    tail->next = node;
    tail = node;
    return event_node_jmp_ctl(ffs(event) - 1, EPOLL_CTL_ADD, node);
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

    abio_event_node_t *current_node;
    for (current_node = head.next; current_node != NULL; current_node = current_node->next) {
        if (current_node->fd == fd) {
            if ((current_node->event_mask & event) != 0) {
                //FIXME Queue request
                return -EAGAIN;
            } else {
                current_node->event_mask |= event;
                return event_node_jmp_ctl(idx, EPOLL_CTL_MOD, current_node);
            }
        }
    }
    return event_node_new(fd, event);
}

void event_node_raise(uint32_t event, abio_event_node_t *node) {
    int idx;
    uint32_t mask;
    while (event) {
        idx = ffs(event) - 1;
        mask = 1u << idx;
        if (node->event_mask & mask) {
            node->event_mask &= ~mask;
            abio_context_swap_from_global(&node->context[idx]->context);
        }
        event &= ~mask;
    }
    if (node->event_mask != (EPOLLET | EPOLLONESHOT)) {
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

int event_node_clean_thread(abthread_t *thread) {
    abio_event_node_t *current_node;
    int is_changed, event_mask, idx, mask, ret;
    for (current_node = head.next; current_node != NULL; current_node = current_node->next) {
        is_changed = 0;
        event_mask = current_node->event_mask;
        while (event_mask) {
            idx = ffs(event_mask) - 1;
            mask = ~(1 << idx);
            if (current_node->context[idx] == thread) {
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
