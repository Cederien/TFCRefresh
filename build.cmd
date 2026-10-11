@echo off
rem Configure + build TFCRefresh (Release). Output: build\TFCRefresh.dll
rem Needs VS 2022 Build Tools (C++), CMake, Ninja and vcpkg (VCPKG_ROOT).
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=C:\Program Files\CMake\bin;C:\Program Files\Git\cmd;%LOCALAPPDATA%\Microsoft\WinGet\Links;%PATH%"
if not defined VCPKG_ROOT set "VCPKG_ROOT=C:\Games\Skyrim Helpers\vcpkg"
cd /d "%~dp0"
cmake --preset release || exit /b 1
cmake --build --preset release || exit /b 1
