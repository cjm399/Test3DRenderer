@echo off
setlocal EnableExtensions

REM Single EXE build (WinMain in win32_platform.cpp). Not a split "game DLL + platform EXE" setup.

REM Compiler flags
REM -MT        : Static CRT
REM -nologo    : Quiet compiler banner
REM -EHa-      : No C++ exceptions
REM -Gm-       : Minimal rebuild off (deprecated switch; kept for parity with your template)
REM -GR-       : No RTTI
REM -WX        : Warnings as errors
REM -W4        : Warning level 4
REM -wd...    : Selected warning suppressions
REM -Oi        : Intrinsics
REM -Od        : No optimizations (debug-style)
REM -FC        : Full path in diagnostics
REM -Z7        : Old-style debug info in .obj

REM Linker
REM -subsystem:windows : Win32 GUI entry (WinMain), not console
REM -incremental:no    : No incremental link
REM -opt:ref           : Drop unreferenced COMDATs

set WarningInfo=-WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -wd4530
set SPSpecificFlags=-DSP_INTERNAL=1 -DSP_SLOW=1
set CommonCompileFlags=-MT -nologo -EHa- -Gm- -GR- %WarningInfo% -Oi -Od %SPSpecificFlags% -FC -Z7

set CommonLinkerFlags=-incremental:no -opt:ref
set CommonLinkedLibraries=user32.lib Gdi32.lib Winmm.lib

if not exist .\build mkdir .\build
pushd .\build

del *.pdb >NUL 2>NUL

REM One compile + link: all translation units -> Test3dRenderer.exe
REM Paths are relative to repo root (parent of .\build after pushd).
cl %CommonCompileFlags% ^
  ..\Test3dRenderer\platform\win32_platform.cpp ^
  ..\Test3dRenderer\game\game.cpp ^
  ..\Test3dRenderer\renderer\renderer.cpp ^
  ..\Test3dRenderer\assets\obj_loader.cpp ^
  -FmTest3dRenderer.map ^
  /link %CommonLinkerFlags% -subsystem:windows %CommonLinkedLibraries% -OUT:Test3dRenderer.exe -PDB:Test3dRenderer_%random%.pdb

popd

REM msbuild "%~dp0Test3dRenderer.sln" /p:Configuration=Debug /p:Platform=x64
