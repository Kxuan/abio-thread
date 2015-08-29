//
// Created by xuan on 8/27/15.
//

#ifndef PROJECT_LIBAB_THREAD_H
#define PROJECT_LIBAB_THREAD_H

#include <sys/ucontext.h>

#define LIBAB_THREAD_STACK_SIZE 8192
#define TABLE_SIZE (8 * sizeof(unsigned))
struct abthread_s;
typedef struct abthread_table_s {
    unsigned map;
    struct abthread_s *threads[TABLE_SIZE];
    struct abthread_table_s *next;
} abthread_table_t;

typedef enum {
    ABTHREAD_STATUS_RUNNING = 0,
    ABTHREAD_STATUS_JOIN = 1,
    ABTHREAD_STATUS_DEAD = 2
} ABTHREAD_STATUS;
typedef struct abthread_s {
    int status;
    void *retval;
    abthread_table_t waitlist;
    ucontext_t context;
} abthread_t;


abthread_t *ab_thread_get_by_context(ucontext_t *context);

abthread_t *ab_thread_current();

void ab_thread_exit(void *retval)__attribute__((__noreturn__));

abthread_t *ab_thread_create(void *(*entry)(void *), void *arg);

void ab_thread_init();
void ab_thread_run();
void ab_thread_fini();


#endif //PROJECT_LIBAB_THREAD_H
