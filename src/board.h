/**
 * CS3210 Parallel Computing: Group Project 1 (MPI Aggressive Queen)
 * National University of Singapore.
 *
 * Board data structure.
 */

#ifndef AQ_BOARD_H_
#define AQ_BOARD_H_

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <assert.h>
#include "log.h"

/**
 * Since each board configuration is stored as a bit string, we only have up
 * to 64-bits to play with, effectively restricting board sizes to a max of 8.
 * 
 * To compensate, we slice up the bit configurations across few uint64_ts.
 * The default value of 25 allows for board sizes up to 40
 * (40 * 40 = 1600 / 64 = 25).
 */
#define AQ_BOARD_SLICES 25

/**
 * Defines the number of simulation boards that can be used to check the number
 * of times a row or column on the board can be attacked.
 *
 * The default of 1024 boards should be sufficient for values of k up to 32 and
 * board sizes up to 32.
 */
#define AQ_SIMULATION_BOARDS 1024

/**
 * A structure that represents a chess board.
 * Contains bookkeeping information.
 */
struct aq_board {
	uint64_t slices[AQ_BOARD_SLICES];
	int size;
	int bits_occupied;
	int slices_occupied;
};

/**
 * Creates a new board.
 */
inline
struct aq_board board_new(int size) {
#ifndef NDEBUG
    int slices_required = ceil(size * size / 64.0);
    assert(slices_required <= AQ_BOARD_SLICES &&
           "Try increasing AQ_BOARD_SLICES?");
#endif

    struct aq_board board = { {0}, 0, 0, 0 };
    board.bits_occupied = size * size;
    board.slices_occupied = (board.bits_occupied + 64 - 1) >> 6;
    board.size = size;
    return board;
}

inline
int board_get_slice_id(struct aq_board *board, int row, int col) {
    int offset = row * board->size + col;
    int slice_id = offset >> 6;
    return slice_id;
}

inline
int board_get_offset_in_slice(struct aq_board *board, int row, int col) {
    int offset = row * board->size + col;
    return offset % 64;
}

/**
 * Checks if a specific position on the board is occupied.
 * Returns zero if position is not occupied, non-zero otherwise.
 */
inline
int board_is_occupied(struct aq_board *board, int row, int col) {
	int slice_id = board_get_slice_id(board, row, col);
	int offset = board_get_offset_in_slice(board, row, col);

	// Select the correct value.
	uint64_t mask = 0x8000000000000000ULL >> offset;
	uint64_t value = board->slices[slice_id] & mask;

	return __builtin_ffsll(value);
}

/**
 * Sets the value of a specified position on the board.
 */
inline
void board_set_occupied(struct aq_board *board, int row, int col) {
    int slice_id = board_get_slice_id(board, row, col);
    int offset = board_get_offset_in_slice(board, row, col);

    // Select the correct value.
    uint64_t mask = 0x8000000000000000ULL >> offset;
    board->slices[slice_id] |= mask;
}

/**
 * Sets the value of a specified row on the board.
 */
inline
void board_set_row_occupied(struct aq_board *board, int row) {
#ifndef NDEBUG
    assert(row < board->size);
#endif

    int start_slice_id = board_get_slice_id(board, row, 0);
    int start_offset = board_get_offset_in_slice(board, row, 0);
    int end_slice_id = board_get_slice_id(board, row, board->size - 1);
    int end_offset = board_get_offset_in_slice(board, row, board->size - 1);

    if (start_slice_id == end_slice_id) {
        // XXX: This effectively limits the board size to 63x63.
        uint64_t mask = (0xFFFFFFFFFFFFFFFFULL >> start_offset) ^
                        ~(0xFFFFFFFFFFFFFFFFULL << (63 - end_offset));
        board->slices[start_slice_id] |= mask;
    } else {
        uint64_t start_mask = (0xFFFFFFFFFFFFFFFFULL >> start_offset);
        uint64_t end_mask = (0xFFFFFFFFFFFFFFFFULL << (63 - end_offset));
        board->slices[start_slice_id] |= start_mask;
        board->slices[end_slice_id] |= end_mask;
    }
}

/**
 * Sets the value of a specified column on the board.
 */
inline
void board_set_col_occupied(struct aq_board *board, int col) {
#ifndef NDEBUG
    assert(col < board->size);
#endif

    // Compute mask for a specified slice.
    for (int i = 0; i < board->slices_occupied; ++i){
        uint64_t mask = 0x8000000000000000ULL;
        for (int j = col; j < 64; j += board->size) {
            mask >>= board->size;
            mask |= 0x8000000000000000ULL;
        }

        // Sometimes we may end up on the same row - we need to correct that.
        int row = (i << 6) / board->size;
        int slice_id = board_get_slice_id(board, row, col);
        if (slice_id != i) {
            row++;
        }

        int offset = (row * board->size + col) % 64 % board->size;
        mask >>= offset;
        board->slices[i] |= mask;
    }
}

/**
 * Sets the value of a specified diagonal on the board.
 */
inline
void board_set_diag_occupied(struct aq_board *board, int row, int col) {
    // I don't think there's an easy fast way to do this...
    // Top left direction.
    int i = row, j = col;
    while (i >= 0 && j >= 0) {
        board_set_occupied(board, i, j);
        i--;
        j--;
    }

    // Top right direction.
    i = row;
    j = col;
    while (i >= 0 && j < board->size) {
        board_set_occupied(board, i, j);
        i--;
        j++;
    }

    // Bottom left direction.
    i = row;
    j = col;
    while (i < board->size && j >= 0) {
        board_set_occupied(board, i, j);
        i++;
        j--;
    }

    // Bottom right direction.
    i = row;
    j = col;
    while (i < board->size && j < board->size) {
        board_set_occupied(board, i, j);
        i++;
        j++;
    }
}

/**
 * Clears the value of a specified position on the board.
 */
inline
void board_set_unoccupied(struct aq_board *board, int row, int col) {
    int slice_id = board_get_slice_id(board, row, col);
    int offset = board_get_offset_in_slice(board, row, col);

    // Select the correct value.
    uint64_t mask = 0x8000000000000000ULL >> offset;
    board->slices[slice_id] &= ~mask;
}

/**
 * Clears all values from the board.
 */
inline
void board_clear(struct aq_board *board) {
    for (int i = 0; i < board->slices_occupied; ++i) {
        board->slices[i] = 0;
    }
}

/**
 * Counts the number of times a position on the board is attackale.
 * Returns -1 if the position is already occupied by a piece.
 */
inline
int board_cell_count_attacks(struct aq_board *board, int row, int col) {
    int attack_count = 0;

    // We short circuit if the slot is already occupied.
    if (board_is_occupied(board, row, col)) {
        return -1;
    }

    // Check occupied positions on row.
    // Left direction.
    for (int i = row; i >= 0; --i) {
        if (board_is_occupied(board, i, col)) {
            attack_count++;
            break;
        }
    }

    // Right direction.
    for (int i = row; i < board->size; ++i) {
        if (board_is_occupied(board, i, col)) {
            attack_count++;
            break;
        }
    }
    
    // Check occupied positions on column.
    // Top direction.
    for (int i = col; i >= 0; --i) {
        if (board_is_occupied(board, row, i)) {
            attack_count++;
            break;
        }
    }

    // Bottom direction.
    for (int i = col; i < board->size; ++i) {
        if (board_is_occupied(board, row, i)) {
            attack_count++;
            break;
        }
    }

    // Check occupied positions on diagonals.
    // Top left direction.
    int i = row, j = col;
    while (i >= 0 && j >= 0) {
        if (board_is_occupied(board, i, j)) {
            attack_count++;
            break;
        }
        
        i--;
        j--;
    }

    // Top right direction.
    i = row;
    j = col;
    while (i >= 0 && j < board->size) {
        if (board_is_occupied(board, i, j)) {
            attack_count++;
            break;
        }
        
        i--;
        j++;
    }

    // Bottom left direction.
    i = row;
    j = col;
    while (i < board->size && j >= 0) {
        if (board_is_occupied(board, i, j)) {
            attack_count++;
            break;
        }
        
        i++;
        j--;
    }

    // Bottom right direction.
    i = row;
    j = col;
    while (i < board->size && j < board->size) {
        if (board_is_occupied(board, i, j)) {
            attack_count++;   
            break;
        }
        
        i++;
        j++;
    }

    return attack_count;
}

/**
 * Returns the maximum number of attacks on every occupied position on the
 * board.
 */
inline
int board_max_attacks(struct aq_board *board) {
    int max_attacks = 0;
    int num_attacks = 0;

    for (int i = 0; i < board->size; ++i) {
        for (int j = 0; j < board->size; ++j) {
            if (board_is_occupied(board, i, j)) {
                struct aq_board simulation_board = *board;
                board_set_unoccupied(&simulation_board, i, j);

                num_attacks = board_cell_count_attacks(&simulation_board, i, j);
                if (num_attacks > max_attacks) {
                    max_attacks = num_attacks;
                }
            }
        }
    }

    return max_attacks;
}

/**
 * Simulates the maximum number of attacks on every occupied position on the
 * board.
 */
inline
int board_simulate_max_attacks(struct aq_board *board, int row, int col) {
    struct aq_board simulation_board = *board;
    board_set_occupied(&simulation_board, row, col);
    return board_max_attacks(&simulation_board);
}

/**
 * Returns non-zero if the number of attacks on every occupied position on the
 * board is the same, zero otherwise.
 */
inline
int board_all_has_same_attacks(struct aq_board *board) {
    int prev_attacks = -1;
    int attacks = 0;

    for (int i = 0; i < board->size; ++i) {
        for (int j = 0; j < board->size; ++j) {
            if (board_is_occupied(board, i, j)) {
                struct aq_board simulation_board = *board;
                board_set_unoccupied(&simulation_board, i, j);

                attacks = board_cell_count_attacks(&simulation_board, i, j);
                if (prev_attacks == -1) {
                    prev_attacks = attacks;
                }

                if (prev_attacks != attacks) {
                    return 0;
                }
            }
        }
    }

    return 1;
}

/**
 * Counts the number of occupied positions on the board.
 */
inline
int board_count_occupied(struct aq_board *board) {
    int count = 0;
    for (int i = 0; i < board->slices_occupied; ++i) {
        // Praise to be god of SSE4.2.
        count += __builtin_popcountll(board->slices[i]);
    }

    return count;
}

/**
 * Checks if two boards are equal.
 */
inline
int boards_are_equal(struct aq_board *b1, struct aq_board *b2) {
#ifndef NDEBUG
    assert(b1->size == b2->size);
#endif

    for (int i = 0; i < b1->size; ++i) {
        if (b1->slices[i] != b2->slices[i]) {
            return 0;
        }
    }

    return 1;
}

/**
 * Pretty-prints the board.
 *
 * There are just so many things wrong with this printing code but nobody ain't
 * got time for that!
 */
inline
void board_print(struct aq_board *board) {
    // Buffer containing format string.
    char buffer[64];

    // Column numbers.
    snprintf(buffer, 64, " %%%ds   ", (board->size / 10) + 2);
    printf(buffer, " ");
    
    snprintf(buffer, 64, "%%%dd", (board->size / 10) + 2);
    for (int i = 0; i < board->size; ++i) {
        printf(buffer, i);
    }
    printf("\n");

    // Header divider.
    snprintf(buffer, 64, " %%%ds   ", (board->size / 10) + 2);
    printf(buffer, " ");
    
    char dashes[16];
    snprintf(buffer, 64, "%%%ds", (board->size / 10) + 2);
    for (int i = 0; i < board->size; ++i) {
        snprintf(dashes, 16, "%.*s", (i / 10) + 1, "--------");
        printf(buffer, dashes);
    }

    // Actual data values are printed here.
    printf("\n");
    for (int i = 0; i < board->size; ++i) {
        snprintf(buffer, 64, " %%%dd | ", (board->size / 10) + 2);
        printf(buffer, i);

        snprintf(buffer, 64, "%%%ds", (board->size / 10) + 2);
        for (int j = 0; j < board->size; ++j) {
            if (board_is_occupied(board, i, j)) {
                printf(buffer, "x");
            } else {
                printf(buffer, "o");
            }
        }

        printf("\n");
    }
    
    printf("\n");
}

#endif /* AQ_BOARD_H_ */

/* vim: set ts=4 sw=4 et: */
