//
// Created by xuan on 8/26/15.
//

#include <stdio.h>
#include "libab_thread.h"

static void *thread_entry(void *arg) {
    printf("Argument %p\n", arg);
    return arg;
}

int test_main() {
    ab_thread_create(thread_entry, (void *) 1);
    ab_thread_create(thread_entry, (void *) 2);
    return 0;
}