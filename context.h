//
// Created by xuan on 8/26/15.
//

#ifndef PROJECT_LOOP_H
#define PROJECT_LOOP_H

ucontext_t *abcontext_get_current(void);

ucontext_t *abcontext_get_master(void);
int abcontext_is_master(void);
int abcontext_switch(ucontext_t *ucp);

int abcontext_switch_to_master(ucontext_t *ucp);

int abcontext_switch_from_master(ucontext_t *ucp);

#endif //PROJECT_LOOP_H
