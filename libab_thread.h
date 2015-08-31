//
// Created by xuan on 8/27/15.
//

#ifndef PROJECT_LIBAB_THREAD_H
#define PROJECT_LIBAB_THREAD_H

#include <sys/ucontext.h>
#include <stdint.h>

#define LIBAB_THREAD_STACK_SIZE 8192
struct abthread_s;

typedef enum {
    ABTHREAD_REQTYPE_WITH_DATA = 1 << 0, //The request has extra data
    ABTHREAD_REQTYPE_PRI = 1 << 2,  //The request must be perform as soon as possible
    ABTHREAD_REQTYPE_NEED_RESPONSE = 1 << 3//The request sender need a response
} ABTHREAD_REQTYPE;

typedef struct abthread_request_node_s {
    uint8_t type;

    uint32_t request; //User-specified request type.
    uint32_t data_len;//data field length

    union {
        //Request sender
        uint32_t local_thread;
        struct {
            int host;
            uint32_t thread;
        } remote;
    };

    void *next_request;
    //Used by linked table
    void *data;
} abthread_request_node_t;

typedef struct abthread_request_table_s {
    struct abthread_s *thread;
    struct abthread_table_node_s *prev, *next;
} abthread_request_table_t;

typedef enum {
    ABTHREAD_STATUS_RUNNING = 1,
    ABTHREAD_STATUS_DEAD,
    ABTHREAD_STATUS_WAIT_IO,
    ABTHREAD_STATUS_WAIT_REQUEST,
} ABTHREAD_STATUS;

typedef struct abthread_s {
    const char *name;
    int status;
    void *data;
    abthread_request_table_t requests;
    ucontext_t context;
} abthread_t;

abthread_t *ab_thread_get_by_context(ucontext_t *context);

abthread_t *ab_thread_current();

#define current_thread (ab_thread_current())

void ab_thread_exit(void *retval)__attribute__((__noreturn__));

#define ab_thread_create(entry, arg) ab_thread_create_with_name(#entry, entry, arg)
//abthread_t *ab_thread_create(void *(*entry)(void *), void *arg);

abthread_t *ab_thread_create_with_name(const char *name, void *(*entry)(void *), void *arg);

void ab_thread_schedule(void);

void *ab_thread_wait_io(void);

void ab_thread_init();

int ab_thread_run();

void ab_thread_fini();

#endif //PROJECT_LIBAB_THREAD_H
