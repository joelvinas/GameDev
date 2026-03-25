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
cl src\AStarGame.cpp /Iinclude /EHsc /std:c++17 /Febin\AStarGame.exe /Foobj\ user32.lib gdi32.lib
bin\AStarGame.exe