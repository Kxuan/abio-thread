//
// Created by xuan on 8/27/15.
//

#include <malloc.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <stdlib.h>
#include "libab_thread.h"
#include "context.h"
#include "event_node.h"

static void ab_thread_entry(void *(*entry)(void *), void *arg) {
    ab_thread_exit(entry(arg));
}

#define container_of(ptr, type, member) ({ \
     const typeof( ((type *)0)->member ) *__mptr = (ptr); \
     (type *)( (char *)__mptr - offsetof(type,member) );})

abthread_t *ab_thread_current() {
    ucontext_t *current_context = abio_context_get_current(),
            *global_context = abio_context_get_global();
    if (current_context == global_context)
        return NULL;

    return container_of(current_context, abthread_t, context);
}

void __attribute__((__noreturn__)) ab_thread_exit(void *retval) {
    abthread_t *current = ab_thread_current();
    if (!current) {
        fprintf(stderr, "ab_thread_exit on MAIN thread!\n");
        abort();
    }
    current->retval = retval;
    event_node_clean_thread(current);
    //TODO How to notify other thread?

    while (1) {
        abio_context_swap_to_global(&current->context);
        fprintf(stderr, "Thread[%p] exited but weakup again!\n", current);
    }
}

/* Clenaup exited threads */
void ab_thread_cleanup() {
    if (abio_context_get_current() != abio_context_get_global()) {
        fprintf(stderr, "ab_thread_cleanup must be called by global thread!\n");
        return;
    }
    //TODO Cleanup threads
    /*munmap(current->stack_base, LIBAB_THREAD_STACK_SIZE);
    current->stack_base = NULL;*/

}

abthread_t *ab_thread_create(void *(*entry)(void *), void *arg) {
    abthread_t *thread = (abthread_t *) malloc(sizeof(abthread_t));
    if (thread != NULL) {
        thread->stack_base = mmap(NULL, LIBAB_THREAD_STACK_SIZE,
                                  PROT_WRITE | PROT_READ,
                                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_STACK,
                                  -1, 0);
        if (thread->stack_base == MAP_FAILED)
            goto fail_free;
        if (getcontext(&thread->context) == -1)
            goto fail_unmap;
        thread->context.uc_stack.ss_sp = ((uint8_t *) thread->stack_base );
        thread->context.uc_stack.ss_size = LIBAB_THREAD_STACK_SIZE;
        thread->context.uc_link = abio_context_get_global();
        makecontext(&thread->context, (void (*)()) ab_thread_entry, 2, entry, arg);
        abio_context_swap(&thread->context);
    }
    return thread;
    fail_unmap:
    munmap(thread->stack_base, LIBAB_THREAD_STACK_SIZE);
    fail_free:
    free(thread);
    return NULL;
}