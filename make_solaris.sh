#!/bin/sh
cc src/tools/txt2c.c -o src/tools/txt2c
src/tools/txt2c src/base.lua src/tools.lua src/driver_gcc.lua src/driver_cl.lua src/driver_cc.lua src/driver_clang.lua > src/internal_base.h
cc src/*.c src/lua/*.c -o bam -I src/lua -lm -lrt -lpthread -ldl -mt -O2 $*
