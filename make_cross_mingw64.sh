#!/bin/sh

case "$(uname)" in
	*BSD) ccorder='clang gcc' ;;
	   *) ccorder='gcc clang' ;;
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
		echo "No host compiler found. Specify host compiler by setting the CC environment variable." >&2
		echo "Example: CC=gcc ./$0" >&2
		exit 1
	fi
fi

# the actual compile
echo "compiling using $CC as host compiler and x86_64-w64-mingw32-gcc as target compiler..." >&2
$CC -Wall -pedantic src/tools/txt2c.c -o src/tools/txt2c
src/tools/txt2c src/base.lua src/tools.lua src/driver_gcc.lua src/driver_clang.lua src/driver_cl.lua src/driver_solstudio.lua src/driver_xlc.lua > src/internal_base.h
x86_64-w64-mingw32-gcc -Wall -DLUA_BUILD_AS_DLL -pedantic src/*.c src/lua/*.c -o bam -I src/lua -lm -lpthread -O2 $*
