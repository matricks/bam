@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1
@gcc -ansi -pedantic -Wall src/tools/txt2c.c -o src/tools/txt2c
if errorlevel 1 goto finish
@src\tools\txt2c.exe src/base.lua src/tools.lua src/driver_gcc.lua src/driver_clang.lua src/driver_cl.lua src/driver_solstudio.lua src/driver_xlc.lua > src/internal_base.h || goto generate_failed
@gcc -pedantic -Wall src/*.c src/lua/*.c -o bam -I src/lua/ %*

:finish
set "build_result=%errorlevel%"
:restore
popd
exit /b %build_result%

:generate_failed
set "build_result=%errorlevel%"
@REM Redirection can fail without setting ERRORLEVEL.
if "%build_result%"=="0" set "build_result=1"
goto restore
