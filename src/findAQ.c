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
#include <mpi.h>

#include "stack.h"
#include "board.h"
#include "move.h"

static const int NUM_REQUIRED_ARGS = 5;
static const int MAX_SOLUTION_SET_SIZE = 4096;
static const int MAX_MPI_PROCS = 64;

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
void expandStackSize();
int readProgramArgs(int, char**, struct program_args*);
static inline void godFunction(struct program_args*);
static inline void gatherResults(int, int, struct aq_board*);

/**
 * We need more stack size than the default.
 */
void expandStackSize() {
    // Set to 64MB.
    const rlim_t stack_size = 64L * 1024L * 1024L;
    struct rlimit rl;
    int result;

    result = getrlimit(RLIMIT_STACK, &rl);
    if (result == 0) {
        if (rl.rlim_cur < stack_size) {
            rl.rlim_cur = stack_size;
            result = setrlimit(RLIMIT_STACK, &rl);
            if (result != 0) {
                fprintf(stderr, "%s: Failed to increase stack size (errno %d)\n",
                        "expandStackSize", result);
            }
        }
    } else {
        fprintf(stderr, "%s: Failed to increase stack size (errno %d)\n",
                "expandStackSize", result);
    }
}

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
    struct aq_move initial_moves[N * N];
    int num_initial_moves = 0;

    MPI_Comm_size(MPI_COMM_WORLD, &mpi_nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    LOG("prepareTaskStack", "MPI_Comm_size=%d, MPI_Comm_rank=%d", mpi_nprocs,
            mpi_rank);

    // Optimization: we only need to find one half the board.
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N - i; ++j) {
            initial_move.row = i;
            initial_move.col = j;
            initial_move.applied = 0;
            
            initial_moves[num_initial_moves++] = initial_move;
        }
    }

    for (int i = 0; i < num_initial_moves; ++i) {
        int proc_id = i % mpi_nprocs;
        if (proc_id == mpi_rank) {
            stack_push(&stack, initial_moves[i]);
        }
    }
    
    return stack;
}

/**
 * Gathers results of the computation.
 */
static inline
void gatherResults(int num_solutions, int max_queens,
        struct aq_board* solution_set) {
    int i, j, k;
    int has_existing_solutions;

    struct aq_board all_solution_set[MAX_SOLUTION_SET_SIZE];
    int all_num_solutions;
    int all_max_queens;

    struct aq_board gathered_solution_set[MAX_MPI_PROCS][MAX_SOLUTION_SET_SIZE];
    int gathered_num_solutions[MAX_MPI_PROCS];
    int gathered_max_queens[MAX_MPI_PROCS];

    // MPI information.
    int mpi_rank;
    int mpi_nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_nprocs);

#ifndef NDEBUG
    assert(mpi_nprocs < MAX_MPI_PROCS &&
           "Consider increasing MAX_MPI_PROCS");
#endif

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
    MPI_Gather(&num_solutions, 1, MPI_INT, &gathered_num_solutions, 1, MPI_INT, 0,
            MPI_COMM_WORLD);
    MPI_Gather(&max_queens, 1, MPI_INT, &gathered_max_queens, 1, MPI_INT, 0,
            MPI_COMM_WORLD);
    MPI_Gather(solution_set, MAX_SOLUTION_SET_SIZE, mpi_aq_board_type,
            &gathered_solution_set[mpi_rank], MAX_SOLUTION_SET_SIZE,
            mpi_aq_board_type, 0,
            MPI_COMM_WORLD);
    MPI_Reduce(&max_queens, &all_max_queens, 1, MPI_INT, MPI_MAX, 0,
            MPI_COMM_WORLD);

    // Integrate all the solutions.
    if (mpi_rank == 0) {
        for (i = 0; i < mpi_nprocs; ++i) {
            LOG("gatherResults", "Number of solutions from %d: %d", i,
                    gathered_num_solutions[i]);
        }

        // Remove the duplicates.
        all_num_solutions = 0;
        for (i = 0; i < mpi_nprocs; ++i) {
            for (j = 0; j < gathered_num_solutions[i] &&
                            gathered_max_queens[i] == all_max_queens; ++j) {
                has_existing_solutions = 0;
                for (k = 0; k < all_num_solutions; ++k) {
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
        printf("Maximum number of queens: %d\n", all_max_queens);
        
        for (i = 0; i < all_num_solutions; ++i) {
            board_print(&all_solution_set[i]);
        }
    }
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
    struct aq_board solution_set[MAX_SOLUTION_SET_SIZE];
    struct aq_move move;
    struct aq_move next_move;
    struct aq_move* undo_move_ptr;
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

    // Perform a depth first search.
    while (!stack_empty(&stack)) {
        move = stack_pop(&stack);

        // Discard impossible moves.
        while (!stack_empty(&stack_applied)) {
            undo_move_ptr = stack_peek_ptr(&stack_applied);
            if (undo_move_ptr->depth >= move.depth) {
                stack_pop(&stack_applied);
                move_undo(&board, undo_move_ptr);
                LOG("godFunction", "Undoing move %d, %d, depth=%d",
                        undo_move_ptr->row, undo_move_ptr->col, depth);
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
                if (move.row != i && move.col != j &&
                    !board_is_occupied(&board, i, j)) {
                    num_attacks = args->w ?
                        board_cell_count_attacks_wrap(&board, i, j) :
                        board_cell_count_attacks(&board, i, j);
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

    gatherResults(num_solutions, max_queens, solution_set);
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
    int retval;

    // But first, let me expand the stack size.
    expandStackSize();

    // Read our arguments!
    retval = readProgramArgs(argc, argv, &args);
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
