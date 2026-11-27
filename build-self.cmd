@echo off
rem Builds wcc.exe with wcc.exe itself, no Visual Studio needed. The new compiler
rem goes to compiler\obj first (the running wcc.exe cannot be overwritten), then
rem replaces wcc.exe in the root. Built by itself again it gives the same bytes.
setlocal
cd /d "%~dp0"
if not exist wcc.exe echo [wcc] no wcc.exe to build with: run build.cmd first & exit /b 1
if not exist compiler\obj mkdir compiler\obj
rem this folder's wcc.exe by its full name: a bare wcc.exe could be the installed one from PATH
"%~dp0wcc.exe" -Icompiler --stack 16 -o compiler\obj\wcc.exe ^
   compiler\wcc.c compiler\winlink.c compiler\tokenize.c compiler\preprocess.c compiler\type.c ^
   compiler\parse.c compiler\codegen.c compiler\asm.c compiler\strip.c compiler\cc.c compiler\rc.c ^
   compiler\peephole.c compiler\format.c compiler\headers.c
if errorlevel 1 exit /b 1
copy /y compiler\obj\wcc.exe wcc.exe >nul
if errorlevel 1 exit /b 1
echo [wcc] OK (built by wcc)
