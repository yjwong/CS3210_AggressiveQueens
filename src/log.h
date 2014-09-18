/**
 * CS3210 Parallel Computing: Group Project 1 (MPI Aggressive Queen)
 * National University of Singapore.
 *
 * A simple logger.
 */

#include <stdio.h>
#include <stdarg.h>

/**
 * Prints a log message to screen.
 */
#ifndef NDEBUG
inline
void LOG(const char* TAG, const char* format, ...) {
    fprintf(stderr, "%s: ", TAG);
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
    fprintf(stderr, "\n");
}
#else
#define LOG(...) 
#endif

/* vim: set ts=4 sw=4 et: */
