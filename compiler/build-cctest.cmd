@echo off
rem Builds cctest.exe: the test harness inherited from DynastyOS (programs run inside it, not as .exe).
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist obj mkdir obj
ml64 /nologo /c /Foobj\jmp.obj jmp.asm
if errorlevel 1 exit /b 1
cl /nologo /O2 /W3 /utf-8 /std:c17 /D_CRT_SECURE_NO_WARNINGS /I. /Foobj\ /Fecctest.exe ^
   host.c tokenize.c preprocess.c type.c parse.c codegen.c asm.c strip.c cc.c rc.c peephole.c format.c ^
   ..\runtime\runtime.c headers.c obj\jmp.obj /link /STACK:16777216
if errorlevel 1 exit /b 1
echo [cctest] OK
