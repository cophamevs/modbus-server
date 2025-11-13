cmake_minimum_required(VERSION 3.10)
project(modbus-server C)

set(CMAKE_C_STANDARD 11)

# Platform-specific settings
if(WIN32)
    message(STATUS "Building for Windows")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -D_WIN32_WINNT=0x0601")
    set(EXECUTABLE_SUFFIX ".exe")
else()
    message(STATUS "Building for Linux/Unix")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fPIC")
    set(EXECUTABLE_SUFFIX "")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.c
    src/core/server_controller.c
    src/adapters/modbus_backend.c
    src/adapters/tcp_adapter.c
    src/adapters/rtu_adapter.c
    src/config/config_loader.c
    src/json/json_command.c
    src/utils/byte_order.c
    src/utils/platform.c
    cJSON/cJSON.c
)

# Try to find libmodbus with pkg-config first
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBMODBUS libmodbus)
endif()

# Fallback: manual search for libmodbus
if(NOT LIBMODBUS_FOUND)
    find_path(LIBMODBUS_INCLUDE_DIR modbus/modbus.h)
    find_library(LIBMODBUS_LIBRARY modbus)
    
    if(LIBMODBUS_INCLUDE_DIR AND LIBMODBUS_LIBRARY)
        set(LIBMODBUS_FOUND TRUE)
        set(LIBMODBUS_INCLUDE_DIRS ${LIBMODBUS_INCLUDE_DIR})
        set(LIBMODBUS_LIBRARIES ${LIBMODBUS_LIBRARY})
    endif()
endif()

# Try to find cJSON with pkg-config first
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CJSON libcjson)
endif()

# Fallback: manual search for cJSON
if(NOT CJSON_FOUND)
    find_path(CJSON_INCLUDE_DIR cjson/cJSON.h cJSON.h)
    find_library(CJSON_LIBRARY cjson)
    
    if(CJSON_INCLUDE_DIR AND CJSON_LIBRARY)
        set(CJSON_FOUND TRUE)
        set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIR})
        set(CJSON_LIBRARIES ${CJSON_LIBRARY})
    endif()
endif()

# Include directories for dependencies
if(LIBMODBUS_INCLUDE_DIRS)
    include_directories(${LIBMODBUS_INCLUDE_DIRS})
endif()

if(CJSON_INCLUDE_DIRS)
    include_directories(${CJSON_INCLUDE_DIRS})
endif()

# Create executable
add_executable(modbus-server${EXECUTABLE_SUFFIX} ${SOURCES})

# Link libraries
if(LIBMODBUS_FOUND)
    target_link_libraries(modbus-server${EXECUTABLE_SUFFIX} PRIVATE ${LIBMODBUS_LIBRARIES})
else()
    message(WARNING "libmodbus not found, trying system library")
    target_link_libraries(modbus-server${EXECUTABLE_SUFFIX} PRIVATE modbus)
endif()

if(CJSON_FOUND)
    target_link_libraries(modbus-server${EXECUTABLE_SUFFIX} PRIVATE ${CJSON_LIBRARIES})
else()
    message(WARNING "cJSON not found, trying system library")
    target_link_libraries(modbus-server${EXECUTABLE_SUFFIX} PRIVATE cjson)
endif()

# Link Windows socket library if on Windows
if(WIN32)
    target_link_libraries(modbus-server${EXECUTABLE_SUFFIX} PRIVATE ws2_32)
    message(STATUS "Linking ws2_32 for Windows")
endif()

# Install
install(TARGETS modbus-server${EXECUTABLE_SUFFIX} DESTINATION bin)

# Print build info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "Executable: modbus-server${EXECUTABLE_SUFFIX}")
