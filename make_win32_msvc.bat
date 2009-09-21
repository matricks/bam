@echo off

@REM check if we already have the tools in the environment
if exist "%VCINSTALLDIR%" (
	goto compile
)

@REM Check for Visual Studio
if exist "%VS90COMNTOOLS%" (
	set VSPATH="%VS90COMNTOOLS%"
	goto set_env
)
if exist "%VS80COMNTOOLS%" (
	set VSPATH="%VS80COMNTOOLS%"
	goto set_env
)

echo You need Microsoft Visual Studio 8 or 9 installed
pause
exit

@ setup the environment
:set_env
call %VSPATH%vsvars32.bat

:compile
@cl /D_CRT_SECURE_NO_DEPRECATE /O2 /nologo src/tools/txt2c.c /Fesrc/tools/txt2c.exe
@src\tools\txt2c src/base.lua src/driver_gcc.lua src/driver_cl.lua > src\internal_base.h
@cl /D_CRT_SECURE_NO_DEPRECATE /W3 /TC /O2 /nologo /I src/lua src/*.c src/lua/*.c /Febam.exe
@del *.obj

