@echo off

@REM Check for Visual Studio
call set "VSPATH="
if defined VS140COMNTOOLS ( if not defined VSPATH (
	call set "VSPATH=%%VS140COMNTOOLS%%"
) )
if defined VS120COMNTOOLS ( if not defined VSPATH (
	call set "VSPATH=%%VS120COMNTOOLS%%"
) )
if defined VS110COMNTOOLS ( if not defined VSPATH (
	call set "VSPATH=%%VS110COMNTOOLS%%"
) )
if defined VS100COMNTOOLS ( if not defined VSPATH (
	call set "VSPATH=%%VS100COMNTOOLS%%"
) )
if defined VS90COMNTOOLS ( if not defined VSPATH (
	call set "VSPATH=%%VS90COMNTOOLS%%"
) )
if defined VS80COMNTOOLS ( if not defined VSPATH (
	call set "VSPATH=%%VS80COMNTOOLS%%"
) )

@REM check if we already have the tools in the environment
if exist "%VCINSTALLDIR%" (
	goto compile
)

if not defined VSPATH (
	echo You need Microsoft Visual Studio 8, 9, 10, 11, 12, 13 or 15 installed
	pause
	exit
)

@REM set up the environment
if exist "%VSPATH%..\..\vc\vcvarsall.bat" (
	call "%%VSPATH%%..\..\vc\vcvarsall.bat" amd64
	goto compile
)

echo Unable to set up the environment
pause
exit

:compile
@echo === building bam ===
@cl /D_CRT_SECURE_NO_DEPRECATE /O2 /nologo src/tools/txt2c.c /Fesrc/tools/txt2c.exe
@src\tools\txt2c src/base.lua src/tools.lua src/driver_gcc.lua src/driver_clang.lua src/driver_cl.lua src/driver_solstudio.lua src/driver_xlc.lua > src\internal_base.h

@REM /DLUA_BUILD_AS_DLL = export lua functions
@REM /W3 = Warning level 3
@REM /Ox = max optimizations
@REM /TC = compile as c
@REM /Zi = generate debug database
@REM /GS- = no stack checks
@REM /GL = Whole program optimization (ltcg)
@REM /LTCG = link time code generation
@cl /D_CRT_SECURE_NO_DEPRECATE /DLUA_BUILD_AS_DLL /W3 /O2 /TC /Zi /GS- /GL /nologo /I src/lua src/*.c src/lua/*.c /Febam.exe /link Advapi32.lib /LTCG

@REM clean up
@del bam.exp
@del *.obj
