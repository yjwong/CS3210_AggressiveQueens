#!/bin/sh
#
# CS3210 Parallel Computing: Group Project 1 (MPI Aggressive Queen)
# National University of Singapore.
#
# This is the main build script.
# To generate a debug build, do ./build.sh debug
# To generate a release build, do ./build.sh release
# To clean the source, do ./build.sh clean
#
# If no arguments are supplied, the default is to do a debug build.

if [ -z "$1" ]; then
    BUILD_TYPE=debug
else
    BUILD_TYPE=$1
fi

if [ $BUILD_TYPE = "debug" ]; then
    CFLAGS="-g -O0"
elif [ $BUILD_TYPE = "release" ]; then
    CFLAGS="-O3 -DNDEBUG"
elif [ $BUILD_TYPE = "clean" ]; then
    make clean
    exit
else
    echo "Build type must be either \"debug\" or \"release\"."
    exit
fi

NUM_CPUS=$(getconf _NPROCESSORS_ONLN)

CFLAGS=$CFLAGS ./configure
make -j $NUM_CPUS

# vim: set ts=4 sw=4 et:
