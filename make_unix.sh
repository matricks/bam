#!/bin/sh

case "$(uname)" in
	*BSD) ccorder='clang gcc'; ldl=       ;;
	   *) ccorder='gcc clang'; ldl='-ldl' ;;
esac

# auto detection of compiler
if [ -z "$CC" ]; then
	for cctry in $ccorder; do
		if $cctry --version > /dev/null 2>&1; then
			CC=$cctry
			break;
		fi
	done

	if [ -z "$CC" ]; then
		echo "No compiler found. Specify compiler by setting the CC environment variable." >&2
		echo "Example: CC=gcc ./$0" >&2
		exit 1
	fi
fi

# the actual compile
echo "compiling using $CC..." >&2
$CC -Wall -pedantic src/tools/txt2c.c -o src/tools/txt2c
src/tools/txt2c src/base.lua src/tools.lua src/driver_gcc.lua src/driver_clang.lua src/driver_cl.lua > src/internal_base.h
$CC -Wall -pedantic src/*.c src/lua/*.c -o bam -I src/lua -lm -lpthread $ldl -O2 -rdynamic $*
