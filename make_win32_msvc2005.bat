@cl /D_CRT_SECURE_NO_DEPRECATE /O2 /nologo src/tools/txt2c.c /Fesrc/tools/txt2c.exe
@src\tools\txt2c.exe < src\base.bam > src\internal_base.h
@cl /D_CRT_SECURE_NO_DEPRECATE /W3 /TC /O2 /nologo /I src/lua/include src/lua/src/*.c src/lua/src/lib/*.c src/*.c /Fesrc/bam.exe
@del *.obj

