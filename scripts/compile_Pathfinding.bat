@echo off
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if exist %VCVARS% goto M1
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
if exist %VCVARS% goto M1
set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
if exist %VCVARS% goto M1
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
if exist %VCVARS% goto M1
echo "Could not find vcvarsall.bat"
exit /b 1

:M1
call %VCVARS% x64
cd /d "%~dp0\.."
cl src\Main.cpp src\MapGeneration.cpp src\Navigation.cpp src\Agent.cpp src\EntityManager.cpp src\TimeOfDay.cpp /Iinclude /I "C:\SDK\SDL\include" /EHsc /std:c++17 /Febin\PathfindingGame.exe /Foobj\ /link /LIBPATH:"C:\SDK\SDL\lib\x64" SDL3.lib user32.lib gdi32.lib shell32.lib /SUBSYSTEM:CONSOLE
bin\PathfindingGame.exe
