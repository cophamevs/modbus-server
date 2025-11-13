#!/bin/bash
# Build script for Linux

set -e

echo "Building modbus-server for Linux..."

# Check dependencies
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake not found. Install with: sudo apt-get install cmake"
    exit 1
fi

if ! command -v gcc &> /dev/null; then
    echo "Error: gcc not found. Install with: sudo apt-get install gcc"
    exit 1
fi

if ! pkg-config --exists libmodbus; then
    echo "Error: libmodbus not found. Install with: sudo apt-get install libmodbus-dev"
    exit 1
fi

# Create build directory
rm -rf build
mkdir -p build

# Configure and build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Create distribution
mkdir -p ../dist
cp modbus-server ../dist/modbus-server-linux-x64
chmod +x ../dist/modbus-server-linux-x64

echo "Build complete: dist/modbus-server-linux-x64"
