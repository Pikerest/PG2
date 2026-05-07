@echo off
setlocal

set VS_BUILD_TOOLS=D:\VSBuildTools
set VCVARS=%VS_BUILD_TOOLS%\VC\Auxiliary\Build\vcvarsall.bat
set VCPKG_ROOT=%VS_BUILD_TOOLS%\VC\vcpkg
set VCPKG_DOWNLOADS=%~dp0.vcpkg-downloads
set CMAKE_EXE=%VS_BUILD_TOOLS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
set NINJA_EXE=%VS_BUILD_TOOLS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe
set EXE=build\pg2.exe

if not exist "%VCVARS%" (
    echo Missing Visual C++ environment: %VCVARS%
    pause
    exit /b 1
)

if not exist "%CMAKE_EXE%" (
    echo Missing CMake: %CMAKE_EXE%
    pause
    exit /b 1
)

if not exist "%NINJA_EXE%" (
    echo Missing Ninja: %NINJA_EXE%
    pause
    exit /b 1
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo Missing vcpkg: %VCPKG_ROOT%\vcpkg.exe
    pause
    exit /b 1
)

call "%VCVARS%" x64
set PATH=%VS_BUILD_TOOLS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS_BUILD_TOOLS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\Program Files\Git\cmd;%PATH%

where git
git --version

echo Configuring project...

"%CMAKE_EXE%" -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
if %ERRORLEVEL% neq 0 (
    echo Configuration failed.
    pause
    exit /b %ERRORLEVEL%
)

"%CMAKE_EXE%" --build build --target pg2
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b %ERRORLEVEL%
)

if exist %EXE% (
    echo Running application...
    %EXE%
) else (
    echo Executable not found: %EXE%
    pause
    exit /b 1
)
