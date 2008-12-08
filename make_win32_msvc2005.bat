@echo off 
 
:: Check for Visual Studio 
if exist "%VS90COMNTOOLS%" ( 
	set VSPATH="%VS90COMNTOOLS%" 
	goto compile 
) 
if exist "%VS80COMNTOOLS%" ( 
	set VSPATH="%VS80COMNTOOLS%" 
	goto compile 
) 
 
echo You need Microsoft Visual Studio 8 or 9 installed 
pause 
exit 
 
:compile 
 
call %VSPATH%vsvars32.bat 

@cl /D_CRT_SECURE_NO_DEPRECATE /O2 /nologo src/tools/txt2c.c /Fesrc/tools/txt2c.exe
@src\tools\txt2c.exe < src\base.bam > src\internal_base.h
@cl /D_CRT_SECURE_NO_DEPRECATE /W3 /TC /O2 /nologo /I src/lua/include src/lua/src/*.c src/lua/src/lib/*.c src/*.c /Fesrc/bam.exe
@del *.obj

