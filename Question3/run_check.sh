#!/bin/sh
set -eu

gcc -Wall -Wextra -Wpedantic -std=c11 -O2 -pthread \
    -o perfect_numbers perfect_numbers.c
./perfect_numbers
