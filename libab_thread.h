//
// Created by xuan on 8/27/15.
//

#ifndef PROJECT_LIBAB_THREAD_H
#define PROJECT_LIBAB_THREAD_H

#include <sys/ucontext.h>

#define LIBAB_THREAD_STACK_SIZE 8192
typedef struct abthread_s {
    void *retval;
    void *stack_base;
    ucontext_t context;
} abthread_t;

abthread_t *ab_thread_current();

void ab_thread_exit(void *retval)__attribute__((__noreturn__));

abthread_t *ab_thread_create(void *(*entry)(void *), void *arg);

void ab_thread_cleanup();

#endif //PROJECT_LIBAB_THREAD_H
