/**
 * CS3210 Parallel Computing: Group Project 1 (MPI Aggressive Queen)
 * National University of Singapore.
 *
 * This is a parallel implementation of a solution to the Aggressive Queens
 * (AQ) problem.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <mpi/mpi.h>

#include "stack.h"
#include "board.h"
#include "move.h"

static const int NUM_REQUIRED_ARGS = 5;

static const int EXIT_OK = 0;
static const int EXIT_NUM_ARGS_INCORRECT = 1;
static const int EXIT_ARGS_INVALID = 2;
static const int EXIT_UNKNOWN = 3;

/**
 * A structure that stores program arguments.
 */
struct program_args {
    int N;
    int k;
    int l;
    int w;
};

/**
 * Function prototypes.
 */
int readProgramArgs(int, char**, struct program_args*);
int isConfigurationSafe(int);
int getNumberOfAttacks(int, int, int);

/**
 * Reads the arguments for the program.
 */
int readProgramArgs(int argc, char* argv[], struct program_args* program_args) {
    assert(program_args != NULL);
    errno = 0;

    if (argc != NUM_REQUIRED_ARGS) {
        fprintf(stderr, "%s: Exactly %d arguments (N, k, l, w) are required.\n",
                argv[0], NUM_REQUIRED_ARGS);
        return EXIT_NUM_ARGS_INCORRECT;
    }

    // Set variable names.
    program_args->N = strtol(argv[1], NULL, 0);
    if (errno) {
        fprintf(stderr, "Error converting 1st argument: %s\n", strerror(errno));
        return EXIT_ARGS_INVALID;
    }

    program_args->k = strtol(argv[2], NULL, 0);
    if (errno) {
        fprintf(stderr, "Error converting 2nd argument: %s\n", strerror(errno));
        return EXIT_ARGS_INVALID;
    }

    program_args->l = strtol(argv[3], NULL, 0);
    if (errno) {
        fprintf(stderr, "Error converting 3rd argument: %s\n", strerror(errno));
        return EXIT_ARGS_INVALID;
    }

    program_args->w = strtol(argv[4], NULL, 0);
    if (errno) {
        fprintf(stderr, "Error converting 4th argument: %s\n", strerror(errno));
        return EXIT_ARGS_INVALID;
    }

    // Check if the values are sane.
    if (program_args->N <= 1) {
        fprintf(stderr, "N must be equal or larger than 3.\n");
        return EXIT_ARGS_INVALID;
    }

    if (program_args->k < 0) {
        fprintf(stderr, "k must be equals to or larger than 0.\n");
        return EXIT_ARGS_INVALID;
    }

    return EXIT_OK;
}

/**
 * Prepares the task stack based on board size.
 */
static inline
struct aq_stack prepareTaskStack(int N) {
    // Prepare a stack of initial moves.
    struct aq_stack stack = stack_new();
    struct aq_move initial_move;
    int mpi_rank;
    int mpi_nprocs;

    MPI_Comm_size(MPI_COMM_WORLD, &mpi_nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    LOG("prepareTaskStack", "MPI_Comm_size=%d, MPI_Comm_rank=%d", mpi_nprocs,
            mpi_rank);

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            int proc_id = (i * N + j) % mpi_nprocs;
            if (proc_id == mpi_rank) {
                initial_move.row = i;
                initial_move.col = j;
                initial_move.applied = 0;
                
                LOG("prepareTaskStack", "[%d] Generating move %d, %d", proc_id, i, j);
                stack_push(&stack, initial_move);
            }
        }
    }
    
    return stack;
}

/**
 * Runs the Aggressive Queens algorithm.
 */
static inline
void godFunction(struct program_args *args) {
    // Use a simple integer for the board - we abuse the bits for queen
    // positioning.
    struct aq_board board = board_new(args->N);

    // Prepare the task stack.
    struct aq_stack stack = prepareTaskStack(args->N);
    struct aq_stack stack_applied = stack_new();
    struct aq_board solution_set[4096];
    struct aq_move move;
    struct aq_move next_move;
    struct aq_move undo_move;
    int num_solutions = 0;
    int num_queens = 0;
    int max_queens = 0;
    int num_attacks = 0;
    int moves_generated = 0;
    int has_existing_solutions = 0;
    int depth = 0;
    int i = 0;
    int j = 0;

    // MPI information.
    int mpi_rank;
    int mpi_nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_nprocs);

    // Some debugging information...
    LOG("godFunction", "Board info: bits_occupied = %d, slices_occupied = %d\n",
            board.bits_occupied, board.slices_occupied);

    stack_dump(&stack);
    // Perform a depth first search.
    while (!stack_empty(&stack)) {
        LOG("godFunction", "Dump of working stack:");
        //stack_dump(&stack);
        LOG("godFunction", "Dump of applied stack:");
        //stack_dump(&stack_applied);
        move = stack_pop(&stack);
        if (!move.applied) {
            // Discard impossible moves.
            while (!stack_empty(&stack_applied)) {
                undo_move = stack_peek(&stack_applied);
                if (undo_move.depth >= move.depth) {
                    stack_pop(&stack_applied);
                    move_undo(&board, &undo_move);
                    LOG("godFunction", "Undoing move %d, %d, depth=%d", undo_move.row,
                            undo_move.col, depth);
                } else {
                    depth--;
                    break;
                }
            }
            
            // We only apply if we won't get attacked.
            LOG("godFunction", "Applying move %d, %d, depth=%d, move.depth=%d",
                    move.row, move.col, depth, move.depth);
            move_apply(&board, &move, move.depth);
            stack_push(&stack_applied, move);
            depth = move.depth;
            //board_print(&board);
            
            // Accumate solutions.
            num_queens = board_count_occupied(&board);
            if (num_queens >= max_queens &&
                board_max_attacks(&board) == args->k &&
                board_all_has_same_attacks(&board)) {
                LOG("godFunction", " ^ this is a solution");
                if (num_queens > max_queens) {
                    num_solutions = 1;
                    solution_set[0] = board;
                    max_queens = num_queens;
                } else {
                    has_existing_solutions = 0;
                    for (i = 0; i < num_solutions; ++i) {
                        if (boards_are_equal(&solution_set[i], &board)) {
                            has_existing_solutions = 1;
                        }
                    }

                    if (!has_existing_solutions) {
                        solution_set[num_solutions] = board;
                        num_solutions++;
                    }
                }
            }

            // Generate moves.
            moves_generated = 0;
            for (i = 0; i < args->N; ++i) {
                for (j = 0; j < args->N; ++j) {
                    // Even though some of these conditions imply each other,
                    // they are included for performance reasons.
                    if (!board_is_occupied(&board, i, j)) {
                            num_attacks = board_cell_count_attacks(&board, i, j);
                            if (num_attacks != -1 && num_attacks <= args->k &&
                                board_simulate_max_attacks(&board, i, j) <= args->k) {
                            next_move.row = i;
                            next_move.col = j;
                            next_move.applied = 0;
                            next_move.depth = depth + 1;

                            LOG("godFunction", "Generating move %d, %d, depth=%d", i, j, next_move.depth);
                            stack_push(&stack, next_move);
                            moves_generated++;
                        }
                    }
                }
            }

            // No more moves can be generated. Let's backtrack!
            if (!moves_generated) {
                undo_move = stack_pop(&stack_applied);
                move_undo(&board, &undo_move);
                LOG("godFunction", "No more moves, undoing move %d, %d, depth=%d",
                        undo_move.row, undo_move.col, depth);
            } else {
                depth++;
            }

        }
    }

    // Declare a new type to MPI.
    struct aq_board mpi_board;
    MPI_Datatype mpi_aq_board_type;
    int mpi_aq_board_blocklen[4] = { AQ_BOARD_SLICES, 1, 1, 1 };
    MPI_Datatype mpi_aq_board_blocktype[4] = {
        MPI_UINT64_T,
        MPI_INT,
        MPI_INT,
        MPI_INT
    };
    MPI_Aint mpi_aq_board_disp[4];
    mpi_aq_board_disp[0] = (void*) &mpi_board.slices - (void*) &mpi_board;
    mpi_aq_board_disp[1] = (void*) &mpi_board.size - (void*) &mpi_board;
    mpi_aq_board_disp[2] = (void*) &mpi_board.bits_occupied - (void*) &mpi_board;
    mpi_aq_board_disp[3] = (void*) &mpi_board.slices_occupied - (void*) &mpi_board;
    MPI_Type_create_struct(4, mpi_aq_board_blocklen, mpi_aq_board_disp, 
            mpi_aq_board_blocktype, &mpi_aq_board_type);
    MPI_Type_commit(&mpi_aq_board_type);

    // Send the solution set over.
    struct aq_board gathered_solution_set[mpi_nprocs][4096];
    int gathered_num_solutions[mpi_nprocs];
    MPI_Gather(&num_solutions, 1, MPI_INT, &gathered_num_solutions, 1, MPI_INT, 0,
            MPI_COMM_WORLD);
    MPI_Gather(solution_set, 4096, mpi_aq_board_type,
            &gathered_solution_set[mpi_rank], 4096, mpi_aq_board_type, 0,
            MPI_COMM_WORLD);

    // Integrate all the solutions.
    if (mpi_rank == 0) {
        for (int i = 0; i < mpi_nprocs; ++i) {
            printf("Number of solutions from %d: %d\n", i, gathered_num_solutions[i]);
        }

        // Remove the duplicates.
        struct aq_board all_solution_set[4096];
        int all_num_solutions = 0;
        for (i = 0; i < mpi_nprocs; ++i) {
            for (j = 0; j < gathered_num_solutions[i]; ++j) {
                has_existing_solutions = 0;
                for (int k = 0; k < all_num_solutions; ++k) {
                    if (boards_are_equal(&gathered_solution_set[i][j], &all_solution_set[k])) {
                        has_existing_solutions++;
                        break;
                    }
                }

                if (!has_existing_solutions) {
                    all_solution_set[all_num_solutions] = gathered_solution_set[i][j];
                    all_num_solutions++;
                }
            }
        }

        printf("Number of solutions: %d\n", all_num_solutions);
        printf("Maximum number of queens: %d\n", max_queens);
        
        for (i = 0; i < all_num_solutions; ++i) {
            board_print(&all_solution_set[i]);
        }
    }
}

/**
 * The algorithm is given 4 values: N and k, and the two controls l and w.
 * The meaning of the values are as follows:
 *
 * N denotes the dimensions of the chess board (NxN).
 * k denotes the exact number of other queens that can be attacked by any queen
 * l denotes the display mode.
 *     If l is zero, just the maximum values are displayed.
 *     If l is non-zero, the location of the queens are also returned.
 * w denotes the mode of the board.
 *     If w is zero, a normal board is used. 
 *     If w is non-zero, a wrap-around board is used.
 */
int main(int argc, char* argv[]) {
    struct program_args args;
    int retval = readProgramArgs(argc, argv, &args);
    if (retval != EXIT_OK) {
        return retval;
    }

    // We've got everything we need!
    LOG(argv[0], "Received arguments: N = %d, k = %d, l = %d, w = %d", args.N,
        args.k, args.l, args.w);
    
    // Initialize MPI.
    LOG(argv[0], "Initializing MPI...");
    MPI_Init(&argc, &argv);
    
    // Run the AQ solver.
    godFunction(&args);

    MPI_Finalize();
    return EXIT_OK;
}

/* vim: set ts=4 sw=4 et: */
