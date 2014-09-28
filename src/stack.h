/**
 * CS3210 Parallel Computing: Group Project 1 (MPI Aggressive Queen)
 * National University of Singapore.
 *
 * A stack implementation in C.
 */

#ifndef AQ_STACK_H_
#define AQ_STACK_H_

#include <errno.h>
#include "move.h"

/**
 * Since we're using a backtracking algorithm for the chess board, the maximum
 * depth of a branch would be NxN. The default value here (2048) should have
 * no issues handling boards sizes up to 40x40.
 *
 * Once may ask, why not just allocate memory dynamically? Dynamic allocation
 * happens on the heap. This may increase cache misses during manipulation of
 * the stack, so we stick to a stack-based implementation.
 */
#define AQ_STACK_SIZE 2048

/**
 * A structure that represents a stack.
 * Contains the stack and bookkeeping information.
 */
struct aq_stack {
    struct aq_move stack[AQ_STACK_SIZE];
    int top;
};

/**
 * Creates a stack.
 */
inline
struct aq_stack stack_new() {
    struct aq_stack stack;
    stack.top = -1;
    return stack;
}

/**
 * Pops an item off the stack.
 *
 * If there are no items in the stack, errno is set, and the return value is
 * NULL. It is the responsibility of callers to check the value of errno
 * to ensure that there are no error conditions.
 */
inline
struct aq_move stack_pop(struct aq_stack *stack) {
    int top = stack->top;
    if (top < 0) {
        struct aq_move move;
        errno = EFAULT;
        return move;
    } else {
        struct aq_move item = stack->stack[top];
        stack->top = top - 1;
        return item;
    }
}

/**
 * Pushes an item onto the stack.
 * If the stack is full, errno is set.
 */
inline
void stack_push(struct aq_stack *stack, struct aq_move item) {
    int new_top = stack->top + 1;
    if (new_top == AQ_STACK_SIZE) {
        errno = EFAULT;
    } else {
        stack->top = new_top;
        stack->stack[new_top] = item;
    }
}

/**
 * Peeks into the stack.
 */
inline
struct aq_move stack_peek(struct aq_stack *stack) {
    int top = stack->top;
    if (top < 0) {
        struct aq_move move;
        errno = EFAULT;
        return move;
    } else {
        struct aq_move item = stack->stack[top];
        return item;
    }
}

inline
struct aq_move* stack_peek_ptr(struct aq_stack* stack) {
    int top = stack->top;
    if (top < 0) {
        return NULL;
    } else {
        return &stack->stack[top];
    }
}

/**
 * Clears the stack.
 */
inline
void stack_clear(struct aq_stack *stack) {
    stack->top = -1;
}

/**
 * Returns zero if the stack is not empty, non-zero otherwise.
 */
inline
int stack_empty(struct aq_stack *stack) {
    return stack->top < 0;
}

/**
 * Returns the number of items in the stack.
 */
inline
int stack_count(struct aq_stack *stack) {
    return stack->top + 1;
}

/**
 * Dumps the contents of a stack.
 */
inline
void stack_dump(struct aq_stack *stack) {
    for (int i = 0; i <= stack->top; ++i) {
        LOG("stack_dump", "row=%d, col=%d, depth=%d", stack->stack[i].row,
                stack->stack[i].col, stack->stack[i].depth);
    }
}

#endif /* AQ_STACK_H_ */

/* vim: set ts=4 sw=4 et: */
