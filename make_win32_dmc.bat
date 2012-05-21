@REM --- Batch file that builds bam using Digital Mars C Compiler

@dmc -L/NOL -A src\tools\txt2c.c -o src\tools\txt2c.exe
@src\tools\txt2c.exe src\base.lua src\tools.lua src\driver_gcc.lua src/driver_clang.lua src\driver_cl.lua > src\internal_base.h

@REM ------ Generate fileliest
@move src\tools\txt2c.c src\tools\txt2c.c.temp > nul
@dir /s /b src\*.c > files
@move src\tools\txt2c.c.temp src\tools\txt2c.c > nul

@dmc -Isrc/lua @files -o bam.exe

@REM ------ Restore everything
@del *.obj *.map
@del files

