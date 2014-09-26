/**
 * CS3210 Parallel Computing: Group Project 1 (MPI Aggressive Queen)
 * National University of Singapore.
 *
 * Move data structure.
 */

#ifndef AQ_MOVE_H_
#define AQ_MOVE_H_

#include "board.h"

/**
 * A structure that represents move on the chess board.
 */
struct aq_move {
    int row;
    int col;
    int applied;
    int depth;
};

/**
 * Applies a move to a specific board.
 */
inline
void move_apply(struct aq_board *board, struct aq_move *move, int depth) {
    board_set_occupied(board, move->row, move->col);
    move->applied = 1;
    move->depth = depth;
}

/**
 * Undo a move to a specific board.
 */
inline
void move_undo(struct aq_board *board, struct aq_move *move) {
    board_set_unoccupied(board, move->row, move->col);
    move->applied = 0;
}

#endif /* AQ_MOVE_H_ */

/* vim: set ts=4 sw=4 et: */
