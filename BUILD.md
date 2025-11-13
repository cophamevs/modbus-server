# Modbus JSON Server

A modular, cross-platform Modbus TCP/RTU server with JSON command interface.

## Features

- **Cross-platform**: Supports Windows, Linux, and other Unix-like systems
- **Modular architecture**: Clean separation of concerns
- **Dual mode**: TCP server and RTU slave support
- **JSON interface**: Control and monitor via JSON commands
- **Auto-reconnect**: Automatic RTU connection recovery
- **Multi-client**: Support for multiple TCP clients simultaneously

## Building

### Prerequisites

#### Linux (Ubuntu 22.04)
```bash
sudo apt-get install -y libmodbus-dev cmake gcc make pkg-config
```

#### Windows (MSYS2)
```bash
# Run in MSYS2 MinGW64 terminal
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make mingw-w64-x86_64-libmodbus-git mingw-w64-x86_64-cjson --noconfirm
```

### Build Instructions

#### Using CMake (Recommended for all platforms)

```bash
# Create build directory
mkdir build
cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)       # Linux/macOS
# OR
mingw32-make -j$(nproc)  # Windows MSYS2
```

#### Using Build Scripts

**Linux:**
```bash
chmod +x build-linux.sh
./build-linux.sh
```

**Windows:**
```bash
build-windows.bat
```

#### Using Makefile (Alternative for Windows MSYS2)

```bash
cd /d/VBA/Dev/modbus-server
mingw32-make clean
mingw32-make
```

The binary will be at: `build/bin/modbus-server.exe` (Windows) or `build/modbus-server` (Linux)

## Project Structure

```
modbus-server/
├── src/
│   ├── main.c                      # Entry point
│   ├── core/
│   │   ├── server_controller.h/c   # Main server logic
│   ├── adapters/
│   │   ├── modbus_backend.h        # Backend structure
│   │   ├── tcp_adapter.h/c         # TCP server
│   │   └── rtu_adapter.h/c         # RTU slave
│   ├── config/
│   │   ├── config.h/config_loader.c
│   ├── json/
│   │   ├── json_command.h/c        # JSON command processing
│   └── utils/
│       ├── logging.h               # Debug logging
│       └── byte_order.h/c          # Byte order handling
├── include/cJSON/                  # cJSON headers
├── cJSON/                          # cJSON library
├── CMakeLists.txt                  # CMake build config
├── Makefile                        # Makefile build config
├── build-linux.sh                  # Linux build script
└── build-windows.bat               # Windows build script
```

## CI/CD

GitHub Actions automatically builds the project for:
- Linux (Ubuntu 22.04) - x64
- Windows - x64

Releases are created from git tags (v*.*.*).

### To create a release:

```bash
git tag v1.0.0
git push origin v1.0.0
```

Binaries will be automatically built and uploaded to GitHub Releases.

## Configuration

Configuration is loaded from `modbus_config.json`. Example:

```json
{
  "mode": "tcp_rtu",
  "tcp_port": 1502,
  "unit_id": 1,
  "serial": {
    "device": "/dev/ttyUSB0",
    "baudrate": 9600,
    "parity": "N",
    "data_bits": 8,
    "stop_bits": 1
  },
  "coils": [0, 100],
  "input_bits": [0, 100],
  "holding_registers": [0, 100],
  "input_registers": [0, 100]
}
```

## JSON Command Interface

### Start Server
```json
{"cmd": "start"}
```

### Stop Server
```json
{"cmd": "stop"}
```

### Get Status
```json
{"cmd": "status"}
```

### Update Register Data
```json
{
  "type": "coil",
  "address": 10,
  "datatype": "uint16",
  "byte_order": "LE",
  "value": 1234
}
```

## License

See LICENSE file for details.

## Contributing

Pull requests are welcome. For major changes, please open an issue first to discuss proposed changes.
