#!/bin/sh
gcc -Wall -ansi -pedantic src/tools/txt2c.c -o src/tools/txt2c
src/tools/txt2c src/base.bam src/driver_gcc.bam src/driver_cl.bam > src/internal_base.h
gcc -Wall -ansi -pedantic src/lua/*.c src/*.c -o src/bam -I src/lua -lpthread -O2 $*
