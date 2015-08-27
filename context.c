//
// Created by xuan on 8/26/15.
//

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ucontext.h>
#include "event_node.h"
#include "test.h"

#define EPOLL_EVENT_MAX 100

static int abio_pollfd = -1;
static struct epoll_event events[EPOLL_EVENT_MAX];
static ucontext_t global_context, *current_context = &global_context;

ucontext_t *abio_context_get_current(void) {
    return current_context;
}

ucontext_t *abio_context_get_global(void) {
    return &global_context;
}

int abio_context_swap(ucontext_t *ucp) {
    ucontext_t *oucp = current_context;
    current_context = ucp;
    return swapcontext(oucp, ucp);
}

int abio_context_swap_to_global(ucontext_t *ucp) {
    current_context = &global_context;
    return swapcontext(ucp, &global_context);
}

int abio_context_swap_from_global(ucontext_t *ucp) {
    current_context = ucp;
    return swapcontext(&global_context, ucp);
}

static void abio_once() {
    int ready, i;
    if (abio_pollfd < 0) {
        fprintf(stderr, "abio not initialized\n");
        abort();
    }
    ready = epoll_wait(abio_pollfd, events, EPOLL_EVENT_MAX, -1);
    if (ready == 0) {
        //Timeout
        fprintf(stderr, "epoll_wait TIMEOUT\n");
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
            event_node_raise(events[i].events, events[i].data.ptr);
        }
    }
}

void abio_loop() {
    while (!event_node_empty()) {
        abio_once();
        ab_thread_cleanup();
    }
}

int main() {
    abio_pollfd = epoll_create1(EPOLL_CLOEXEC);
    if (abio_pollfd < 0) {
        perror("epoll_create");
        return 1;
    }
    event_node_init(abio_pollfd);
    test_main();
    abio_loop();
    event_node_fini();
    return 0;
}
