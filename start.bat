@echo off
REM Set up Visual Studio 2026 environment for Ninja
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x86

set PRESET=vcpkg
set EXE=build\pg2.exe

echo Building project using preset: %PRESET%...

cmake --preset %PRESET%
if %ERRORLEVEL% neq 0 (
    echo Configuration failed! Cleaning build directory and retrying...
    if exist build (
        rd /s /q build
    )
    cmake --preset %PRESET%
    if %ERRORLEVEL% neq 0 (
        echo Configuration failed again!
        pause
        exit /b %ERRORLEVEL%
    )
)

cmake --build build --target pg2
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
