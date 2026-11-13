#!/bin/sh
set -eu

gcc -Wall -Wextra -Wpedantic -std=c11 -O2 -o pipeline pipeline.c
strace -f -tt -o trace.log ./pipeline
strace -f -e trace=clone,fork,execve,pipe,pipe2,dup2,open,openat,read,write,close,wait4 \
    -o trace-filtered.log ./pipeline
