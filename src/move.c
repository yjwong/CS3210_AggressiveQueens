/**
 * CS3210 Parallel Computing: Group Project 1 (MPI Aggressive Queen)
 * National University of Singapore.
 *
 * Move data structure.
 */

#include "move.h"

extern void move_apply(struct aq_board*, struct aq_move*, int depth);
extern void move_undo(struct aq_board*, struct aq_move*);

/* vim: set ts=4 sw=4 et: */
