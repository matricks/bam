@cl /D_CRT_SECURE_NO_DEPRECATE /W3 /O2 /nologo src/*.c src/lua/src/*.c src/lua/src/lib/*.c /I src/lua/include /Febam.exe
@del *.obj
