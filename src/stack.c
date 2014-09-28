/**
 * CS3210 Parallel Computing: Group Project 1 (MPI Aggressive Queen)
 * National University of Singapore.
 *
 * A stack implementation in C.
 */

#include "stack.h"

extern struct aq_stack stack_new();
extern struct aq_move stack_pop(struct aq_stack*);
extern void stack_push(struct aq_stack*, struct aq_move);
extern struct aq_move stack_peek(struct aq_stack*);
extern struct aq_move* stack_peek_ptr(struct aq_stack*);
extern void stack_clear(struct aq_stack*);
extern int stack_empty(struct aq_stack*);
extern int stack_count(struct aq_stack*);
extern void stack_dump(struct aq_stack*);

/* vim: set ts=4 sw=4 et: */
