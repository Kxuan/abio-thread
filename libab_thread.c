//
// Created by xuan on 8/27/15.
//

#include <malloc.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "libab_thread.h"
#include "context.h"


typedef struct abthread_table_node_s {
    struct abthread_s *thread;
    struct abthread_table_node_s *prev, *next;
} abthread_table_node_t;

typedef struct abthread_table_s {
    uint32_t count;
    abthread_table_node_t *first, *last;
} abthread_table_t;

#define FOR_EACH_THREAD(table, iter, iter_thread) \
    for((iter) = (table)->first; \
        (iter) != NULL&&((iter_thread)=(iter)->thread); \
        (iter) = (iter)->next)

static void abt_init(abthread_table_t *table) {
    assert(table != NULL);

    memset(table, 0, sizeof(abthread_table_t));
}

static abthread_table_t *abt_alloc() {
    abthread_table_t *table = (abthread_table_t *) malloc(sizeof(abthread_table_t));
    if (table != NULL)
        abt_init(table);
    return table;
}

static abthread_table_node_t *abt_alloc_node(abthread_t *thread) {
    abthread_table_node_t *node = (abthread_table_node_t *) malloc(sizeof(abthread_table_node_t));
    if (node) {
        node->thread = thread;
        node->next = NULL;
        node->prev = NULL;
    }
    return node;
}

static void abt_free_node(abthread_table_node_t *node) {
    free(node);
}

static void abt_remove_all(abthread_table_t *table) {
    abthread_table_node_t *current_node = table->first, *next;
    while (current_node) {
        next = current_node->next;
        abt_free_node(current_node);
        current_node = next;
    }
    table->count = 0;
    table->first = table->last = NULL;
}

static void abt_free(abthread_table_t *table) {
    abt_remove_all(table);
    free(table);
}


static int abt_insert(abthread_table_t *table, abthread_t *thread) {
    assert(table != NULL);
    abthread_table_node_t *node = abt_alloc_node(thread);
    if (!node) {
        return -ENOMEM;
    }

    node->prev = table->last;

    if (table->last) {
        table->last->next = node;
    } else {
        table->first = node;
    }

    table->last = node;
    table->count++;
    return 0;
}

static void abt_remove(abthread_table_t *table, abthread_t *thread) {
    abthread_table_node_t *node = table->first;
    while (node) {
        if (node->thread == thread) {
            if (node == table->first)
                table->first = node->next;

            if (node == table->last)
                table->last = node->prev;

            if (node->prev)
                node->prev->next = node->next;

            if (node->next)
                node->next->prev = node->prev;
            abt_free_node(node);
            table->count--;
            return;
        }
        node = node->next;
    }
    fprintf(stderr, "Could not find Thread-%s in Table-%p\n", thread->name, table);
    abort();
}

static void abt_move(abthread_table_t *table_from, abthread_table_t *table_to, abthread_t *thread) {
    assert(table_from != NULL);
    assert(table_to != NULL);

    abthread_table_node_t *node = table_from->first;
    while (node) {
        if (node->thread == thread) {
            if (node == table_from->first)
                table_from->first = node->next;

            if (node == table_from->last)
                table_from->last = node->prev;

            if (node->prev)
                node->prev->next = node->next;

            if (node->next)
                node->next->prev = node->prev;

            table_from->count--;
            table_to->count++;

            node->prev = table_to->last;
            node->next = NULL;

            if (table_to->last) {
                table_to->last->next = node;
            } else {
                table_to->first = node;
            }

            table_to->last = node;
            return;
        }
        node = node->next;
    }
    fprintf(stderr, "Could not find Thread-%p in Table-%p\n", thread, table_from);
    abort();
}

static abthread_table_t *table_running, *table_waiting_io, *table_dead;
static int has_init = 0;

void ab_thread_init() {
    assert(abcontext_is_master());
    assert(has_init == 0);

    table_running = abt_alloc();
    table_waiting_io = abt_alloc();
    table_dead = abt_alloc();

    has_init = 1;
}

void ab_thread_fini() {
    assert(abcontext_is_master());
    assert(has_init == 1);

    //FIXME Kill all thread

    abt_free(table_dead);
    table_dead = NULL;
    abt_free(table_waiting_io);
    table_waiting_io = NULL;
    abt_free(table_running);
    table_running = NULL;

    has_init = 0;
}

int ab_thread_run() {
    assert(abcontext_is_master());
    abthread_table_node_t *iter;
    abthread_t *thread;
    while (table_running->count) {
        abcontext_switch_from_master(&table_running->first->thread->context);
    }

    if (table_dead->count) {
        FOR_EACH_THREAD(table_dead, iter, thread) {
            //TODO cleanup waitlist
            fprintf(stderr, "Thread %s exit with code %p\n", thread->name, thread->data);

            munmap(thread->context.uc_stack.ss_sp, thread->context.uc_stack.ss_size);
            free(thread);
        }
        abt_remove_all(table_dead);
    }

    return 0;
}

abthread_t *ab_thread_get_by_context(ucontext_t *context) {
    return (abthread_t *) ((char *) context - offsetof(abthread_t, context));
}

abthread_t *ab_thread_current() {
    ucontext_t *current_context = abcontext_get_current(),
            *global_context = abcontext_get_master();

    if (current_context == global_context)
        return NULL;

    return ab_thread_get_by_context(current_context);
}

void __attribute__((__noreturn__)) ab_thread_exit(void *retval) {
    abthread_t *current = current_thread;
    if (!current) {
        fprintf(stderr, "ab_thread_exit on MAIN thread!\n");
        abort();
    }
    current->data = retval;
    current->status = ABTHREAD_STATUS_DEAD;

    abt_move(table_running, table_dead, current);

    while (1) {
        abcontext_switch_to_master(&current->context);
        fprintf(stderr, "Thread-%s dead but weakup again!\n", current->name);
        abort();
    }
}

static void ab_thread_entry(void *(*entry)(void *), void *arg) {
    ab_thread_exit(entry(arg));
}

void ab_thread_schedule(void) {
    abcontext_switch_to_master(&current_thread->context);
}

void *ab_thread_wait_io(void) {
    abthread_t *current = current_thread;

    current->data = NULL;
    current->status = ABTHREAD_STATUS_WAIT_IO;
    abt_move(table_running, table_waiting_io, current);

    ab_thread_schedule();

    abt_move(table_waiting_io, table_running, current);
    current->status = ABTHREAD_STATUS_RUNNING;
    return current->data;
}

void *ab_thread_join(abthread_t *thread) {
    //TODO
    abort();
}


abthread_t *ab_thread_create_with_name(const char *name, void *(*entry)(void *), void *arg) {
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
        thread->context.uc_link = abcontext_get_master();
        makecontext(&thread->context, (void (*)()) ab_thread_entry, 2, entry, arg);

        thread->status = ABTHREAD_STATUS_RUNNING;
        thread->name = name;

        if (abt_insert(table_running, thread) != 0) {
            goto fail_abt_insert;
        }
    }
    return thread;
    fail_abt_insert:
    fail_getcontext:
    munmap(stack_base, LIBAB_THREAD_STACK_SIZE);
    fail_mmap:
    free(thread);
    return NULL;
}
