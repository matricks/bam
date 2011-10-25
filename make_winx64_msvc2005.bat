@echo off

@REM check if we already have the tools in the environment
if exist "%VCINSTALLDIR%" (
	goto compile
)

@REM Check for Visual Studio
if exist "%VS100COMNTOOLS%" (
	set VSPATH="%VS100COMNTOOLS%"
	goto set_env
)
if exist "%VS90COMNTOOLS%" (
	set VSPATH="%VS90COMNTOOLS%"
	goto set_env
)
if exist "%VS80COMNTOOLS%" (
	set VSPATH="%VS80COMNTOOLS%"
	goto set_env
)

echo You need Microsoft Visual Studio 8, 9 or 10 installed
pause
exit

@ setup the environment
:set_env
call %VSPATH%vsvars32.bat

:compile
@cl /D_CRT_SECURE_NO_DEPRECATE /W3 /O2 /nologo src/*.c src/lua/*.c /I src/lua /Febam.exe
@del *.obj
