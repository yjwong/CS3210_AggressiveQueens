/**
 * CS3210 Parallel Computing: Group Project 1 (MPI Aggressive Queen)
 * National University of Singapore.
 *
 * Board data structure.
 */

#include "board.h"

extern struct aq_board board_new(int);
extern int board_get_slice_id(struct aq_board*, int, int);
extern int board_get_offset_in_slice(struct aq_board*, int, int);
extern int board_is_occupied(struct aq_board*, int, int);
extern void board_set_occupied(struct aq_board*, int, int);
extern void board_set_row_occupied(struct aq_board*, int);
extern void board_set_col_occupied(struct aq_board*, int);
extern void board_set_diag_occupied(struct aq_board*, int, int);
extern void board_set_unoccupied(struct aq_board*, int, int);
extern int board_is_attackable(struct aq_board*, int, int);
extern int board_count_occupied(struct aq_board*);
extern int boards_are_equal(struct aq_board*, struct aq_board*);
extern void board_print(struct aq_board*);

/* vim: set ts=4 sw=4 et: */
