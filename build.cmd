@echo off
rem Builds wcc.exe (the WinCC compiler) into this folder with MSVC (Visual Studio 2026).
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist compiler\obj mkdir compiler\obj
ml64 /nologo /c /Focompiler\obj\jmp.obj compiler\jmp.asm
if errorlevel 1 exit /b 1
cl /nologo /O2 /W3 /utf-8 /std:c17 /D_CRT_SECURE_NO_WARNINGS /Icompiler /Focompiler\obj\ /Fewcc.exe ^
   compiler\wcc.c compiler\winlink.c compiler\tokenize.c compiler\preprocess.c compiler\type.c ^
   compiler\parse.c compiler\codegen.c compiler\asm.c compiler\strip.c compiler\cc.c compiler\rc.c compiler\peephole.c compiler\format.c compiler\headers.c ^
   runtime\runtime.c compiler\obj\jmp.obj /link /STACK:16777216
if errorlevel 1 exit /b 1
echo [wcc] OK
