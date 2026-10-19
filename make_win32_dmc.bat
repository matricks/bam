@REM --- Batch file that builds bam using Digital Mars C Compiler
@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1

@dmc -L/NOL -A src\tools\txt2c.c -o src\tools\txt2c.exe
if errorlevel 1 goto cleanup
@src\tools\txt2c.exe src\base.lua src\tools.lua src\driver_gcc.lua src/driver_clang.lua src\driver_cl.lua src\driver_solstudio.lua src\driver_xlc.lua > src\internal_base.h || goto generate_failed

@REM ------ Generate fileliest
@move src\tools\txt2c.c src\tools\txt2c.c.temp > nul
if errorlevel 1 goto cleanup
@dir /s /b src\*.c > files
set "build_result=%errorlevel%"
@move src\tools\txt2c.c.temp src\tools\txt2c.c > nul
if errorlevel 1 goto cleanup
if not "%build_result%"=="0" goto restore

@dmc -Isrc/lua @files -o bam.exe %*

@REM ------ Restore everything
:cleanup
set "build_result=%errorlevel%"
:restore
@del *.obj *.map
@del files
popd
exit /b %build_result%

:generate_failed
set "build_result=%errorlevel%"
@REM Redirection can fail without setting ERRORLEVEL.
if "%build_result%"=="0" set "build_result=1"
goto restore
