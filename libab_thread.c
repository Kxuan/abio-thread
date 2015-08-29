//
// Created by xuan on 8/27/15.
//

#include <malloc.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "libab_thread.h"
#include "context.h"
#include "event_node.h"


#define ATT_FLAG_UNSET 1
#define ATT_FLAG_SET 0
#define ATT_FLAG_FILLED ((unsigned) 0)
#define ATT_FLAG_EMPTY ((unsigned) -1)
#define ATT_IS_SET_(table, idx) (!!((table)->map & (1 << (idx))))
#define ATT_IS_SET(table, idx) (ATT_IS_SET_(table,idx) == ATT_FLAG_SET)
#define ATT_GET(table, idx) ((table)->threads[(idx)])
#define ATT_SET(table, idx, thread) ((table)->map &= ~(1 << (idx)), (table)->threads[(idx)] = (thread))
#define ATT_UNSET(table, idx) ((table)->map |= 1 << (idx), (table)->threads[(idx)] = NULL)
#define FOR_EACH_THREAD(table, idx) \
    for((idx) = abt_next(&(table), -1, ATT_FLAG_SET); \
        (idx) >= 0; \
        (idx) = abt_next(&(table), (idx), ATT_FLAG_SET))

static void abt_init(abthread_table_t *table) {
    assert(table != NULL);
    memset(table, 0, sizeof(abthread_table_t));
    table->map = ATT_FLAG_EMPTY;
}

static abthread_table_t *abt_alloc() {
    abthread_table_t *table = (abthread_table_t *) malloc(sizeof(abthread_table_t));
    if (table != NULL)
        abt_init(table);
    return table;
}

static void abt_free(abthread_table_t *table) {
    abthread_table_t *current = table,
            *next;
    while (current) {
        next = current->next;
        free(current);
        current = next;
    }
}

static int abt_next(abthread_table_t **pcurrent_table, int idx, int status) {
    abthread_table_t *ct;
    assert(pcurrent_table != NULL);
    if (*pcurrent_table == NULL)
        return -1;

    ct = *pcurrent_table;
    if (idx < 0) idx = 0;
    else ++idx;

    do {
        while (idx < TABLE_SIZE) {
            if (ATT_IS_SET_(ct, idx) == status)
                goto out;
            ++idx;
        }
        ct = ct->next;
        idx = 0;
    } while (ct);

    // do not reset table ptr

    return -1;
    out:
    *pcurrent_table = ct;
    return idx;
}

static void abt_optimize(abthread_table_t *table) {
    abthread_table_t *unfulfilled_table = table, *current_table = table;
    abthread_t *thread;
    int idx_ut = -1, idx_ct;


    FOR_EACH_THREAD(current_table, idx_ct) {
        thread = ATT_GET(current_table, idx_ct);
        ATT_UNSET(current_table, idx_ct);

        idx_ut = abt_next(&unfulfilled_table, idx_ut, ATT_FLAG_UNSET);

        ATT_SET(unfulfilled_table, idx_ut, thread);
    }

    if (unfulfilled_table->next) {
        abt_free(unfulfilled_table);
        unfulfilled_table->next = NULL;
    }
}

static int abt_insert(abthread_table_t *table, abthread_t *thread) {
    assert(table != NULL);

    abthread_table_t *current_table = table, *prev_table = NULL;
    int idx;

    while (current_table) {
        if (current_table->map != ATT_FLAG_FILLED) {
            idx = ffs(current_table->map) - 1;
            ATT_SET(current_table, idx, thread);
            return 0;
        }
        prev_table = current_table;
        current_table = current_table->next;
    }
    prev_table->next = abt_alloc();
    if (prev_table->next == NULL) {
        return -1;
    }
    current_table = prev_table->next;
    ATT_SET(current_table, 0, thread);//Mark Index 0 is set on map
    return 0;
}

static int abt_remove(abthread_table_t *table, abthread_t *thread) {
    abthread_table_t *current_table = table;
    int idx;
    while (current_table) {
        for (idx = 0; idx < TABLE_SIZE; ++idx) {
            if (ATT_IS_SET(current_table, idx)) {
                if (current_table->threads[idx] == thread) {
                    ATT_UNSET(current_table, idx);
                    return 0;
                }
            }
        }
        current_table = current_table->next;
    }

    return -1;
}

static int thread_dead_count = 0;
static abthread_table_t table_running, table_dead;
static int has_init = 0;

void ab_thread_init() {
    assert(abio_context_is_global());
    assert(has_init == 0);

    abt_init(&table_running);
    abt_init(&table_dead);

    has_init = 1;
}

void ab_thread_fini() {
    assert(abio_context_is_global());
    assert(has_init == 1);

    //FIXME Kill all thread

    abt_free(table_dead.next);
    abt_free(table_running.next);

    has_init = 0;
}

void ab_thread_run() {
    assert(abio_context_is_global());

    if (table_dead.map != ATT_FLAG_EMPTY) {
        abthread_table_t *table = &table_dead;
        abthread_t *thread;
        int idx;
        FOR_EACH_THREAD(table, idx) {
            thread = ATT_GET(table, idx);

            //TODO cleanup waitlist


            fprintf(stderr, "Thread %p exit with code %p\n", thread, thread->retval);

            munmap(thread->context.uc_stack.ss_sp, thread->context.uc_stack.ss_size);
            free(thread);
        }
        thread = NULL;

        abt_free(table_dead.next);
        abt_init(&table_dead);
    }
    //cleanup tables
    if (thread_dead_count >= TABLE_SIZE) {
        abt_optimize(&table_running);
        thread_dead_count = 0;
    }
}

abthread_t *ab_thread_get_by_context(ucontext_t *context) {
    return (abthread_t *) ((char *) context - offsetof(abthread_t, context));
}

abthread_t *ab_thread_current() {
    ucontext_t *current_context = abio_context_get_current(),
            *global_context = abio_context_get_global();

    if (current_context == global_context)
        return NULL;

    return ab_thread_get_by_context(current_context);
}

void __attribute__((__noreturn__)) ab_thread_exit(void *retval) {
    abthread_t *current = ab_thread_current();
    if (!current) {
        fprintf(stderr, "ab_thread_exit on MAIN thread!\n");
        abort();
    }
    current->retval = retval;
    current->status = ABTHREAD_STATUS_DEAD;

    thread_dead_count++;
    abt_remove(&table_running, current);
    abt_insert(&table_dead, current);

    while (1) {
        abio_context_swap_to_global(&current->context);
        fprintf(stderr, "Thread[%p] dead but weakup again!\n", current);
    }
}

static void ab_thread_entry(void *(*entry)(void *), void *arg) {
    ab_thread_exit(entry(arg));
}

void *ab_thread_join(abthread_t *thread) {
    //TODO
}

abthread_t *ab_thread_create(void *(*entry)(void *), void *arg) {
    abthread_t *thread = (abthread_t *) malloc(sizeof(abthread_t));
    void *stack_base;
    if (thread != NULL) {
        stack_base = mmap(NULL, LIBAB_THREAD_STACK_SIZE,
                          PROT_WRITE | PROT_READ,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_STACK,
                          -1, 0);
        if (stack_base == MAP_FAILED)
            goto fail_mmap;
        if (getcontext(&thread->context) == -1)
            goto fail_getcontext;
        thread->context.uc_stack.ss_sp = ((uint8_t *) stack_base);
        thread->context.uc_stack.ss_size = LIBAB_THREAD_STACK_SIZE;
        thread->context.uc_link = abio_context_get_global();
        makecontext(&thread->context, (void (*)()) ab_thread_entry, 2, entry, arg);
        thread->status = ABTHREAD_STATUS_RUNNING;
        if (abt_insert(&table_running, thread) != 0) {
            goto fail_abt_insert;
        }
        abio_context_swap(&thread->context);
    }
    return thread;
    fail_abt_insert:
    fail_getcontext:
    munmap(stack_base, LIBAB_THREAD_STACK_SIZE);
    fail_mmap:
    free(thread);
    return NULL;
}