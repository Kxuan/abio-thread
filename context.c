//
// Created by xuan on 8/26/15.
//

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ucontext.h>
#include <assert.h>
#include "event_node.h"
#include "test.h"

static ucontext_t master_context, *current_context = &master_context;

ucontext_t *abcontext_get_current(void) {
    return current_context;
}

ucontext_t *abcontext_get_master(void) {
    return &master_context;
}

/* swap current context with {ucp} context */
int abcontext_switch(ucontext_t *ucp) {
    assert(ucp != current_context);

    ucontext_t *oucp = current_context;
    current_context = ucp;
    return swapcontext(oucp, ucp);
}

int abcontext_switch_to_master(ucontext_t *ucp) {
    assert(current_context != &master_context);

    current_context = &master_context;
    return swapcontext(ucp, &master_context);
}

int abcontext_switch_from_master(ucontext_t *ucp) {
    assert(current_context == &master_context);

    current_context = ucp;
    return swapcontext(&master_context, ucp);
}

int abcontext_is_master(void) {
    return current_context == &master_context;
}

int main() {
    if (abevent_init())
        exit(1);
    ab_thread_init();

    test_main();
    abevent_loop();

    ab_thread_fini();
    abevent_fini();
    return 0;
}

