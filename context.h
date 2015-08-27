//
// Created by xuan on 8/26/15.
//

#ifndef PROJECT_LOOP_H
#define PROJECT_LOOP_H

void abio_loop();

ucontext_t * abio_context_get_current(void);
ucontext_t * abio_context_get_global(void);

int abio_context_swap(ucontext_t *ucp);

int abio_context_swap_to_global(ucontext_t *ucp);

int abio_context_swap_from_global(ucontext_t *ucp);

#endif //PROJECT_LOOP_H
