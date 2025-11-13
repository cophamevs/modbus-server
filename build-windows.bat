@echo off
REM Build script for Windows using MSYS2

setlocal enabledelayedexpansion

echo Building modbus-server for Windows...

REM Check if MSYS2 is installed
if not exist "C:\msys64\mingw64.exe" (
    echo Error: MSYS2 not found at C:\msys64
    echo Please install MSYS2 from https://www.msys2.org/
    exit /b 1
)

REM Run build in MSYS2 MinGW64 environment
C:\msys64\mingw64.exe -c "cd /d/VBA/Dev/modbus-server && bash -c 'cd build 2>/dev/null || mkdir build; cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -G \"Unix Makefiles\" && mingw32-make -j$(nproc) && mkdir -p ../dist && cp modbus-server.exe ../dist/modbus-server-windows-x64.exe && echo Build complete: dist/modbus-server-windows-x64.exe'"

if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo.
echo Build complete! Binary at: dist\modbus-server-windows-x64.exe
pause
