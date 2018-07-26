#!/bin/sh
cc src/tools/txt2c.c -o src/tools/txt2c
src/tools/txt2c src/base.lua src/tools.lua src/driver_gcc.lua src/driver_cl.lua src/driver_clang.lua src/driver_solstudio.lua src/driver_xlc.lua > src/internal_base.h
cc src/*.c src/lua/*.c -o bam -DLUA_USE_POSIX -I src/lua -lm -lrt -lpthread -ldl -mt -O2 -std=c99 $*
