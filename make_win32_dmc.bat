@REM --- Batch file that builds bam using Digital Mars C Compiler

@dmc -L/NOL -A src\tools\txt2c.c -o src\tools\txt2c.exe
@src\tools\txt2c.exe src\base.bam src\driver_gcc.bam src\driver_cl.bam > src\internal_base.h

@REM ------ Generate fileliest
@move src\tools\txt2c.c src\tools\txt2c.c.temp > nul
@dir /s /b src\*.c > files
@move src\tools\txt2c.c.temp src\tools\txt2c.c > nul

@dmc -Isrc/lua @files -o src\bam.exe

@REM ------ Restore everything
@del *.obj *.map
@del files

