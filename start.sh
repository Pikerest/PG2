#!/bin/bash

# Zjištění operačního systému
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PRESET="msvc-vcpkg"
    EXE="./build/pg2"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    PRESET="windows-vcpkg"
    EXE="./build/pg2.exe"
else
    echo "Unsupported OS: $OSTYPE"
    exit 1
fi

echo "Building project using preset: $PRESET..."

# Pokus o konfiguraci
cmake --preset "$PRESET"
if [ $? -ne 0 ]; then
    echo "Configuration failed! Cleaning build directory and retrying..."
    # Pokud konfigurace selže (např. kvůli změně generátoru), zkusíme smazat build a znovu
    rm -rf build
    cmake --preset "$PRESET"
    if [ $? -ne 0 ]; then
        echo "Configuration failed again!"
        exit 1
    fi
fi

# Sestavení
cmake --build build --target pg2
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# Spuštění
if [ -f "$EXE" ]; then
    echo "Running application (forcing NVIDIA if available)..."
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        $EXE
    else
        $EXE
    fi
else
    echo "Executable not found: $EXE"
    exit 1
fi
