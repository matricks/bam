#!/bin/sh
gcc -Wall -ansi -pedantic src/tools/txt2c.c -o src/tools/txt2c
src/tools/txt2c src/base.lua src/tools.lua src/driver_gcc.lua src/driver_clang.lua src/driver_cl.lua > src/internal_base.h
gcc -Wall -ansi -pedantic src/lua/*.c src/*.c -o bam -I src/lua -lpthread -O2 $*
