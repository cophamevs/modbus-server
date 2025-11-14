CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I./src -I./include -I./cJSON

# Detect platform
ifeq ($(OS),Windows_NT)
    # Windows
    EXE_EXT = .exe
    PLATFORM_LIBS = -lws2_32
else
    # Linux / Unix
    EXE_EXT =
    PLATFORM_LIBS =
endif

# On both platforms, we link libmodbus + libm
LDFLAGS = -lmodbus -lm $(PLATFORM_LIBS)

# Source directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
OBJ_DIR = $(BUILD_DIR)/obj

# Source files
SOURCES = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/core/server_controller.c \
	$(SRC_DIR)/adapters/modbus_backend.c \
	$(SRC_DIR)/adapters/tcp_adapter.c \
	$(SRC_DIR)/adapters/rtu_adapter.c \
	$(SRC_DIR)/config/config_loader.c \
	$(SRC_DIR)/json/json_command.c \
	$(SRC_DIR)/utils/byte_order.c \
	$(SRC_DIR)/utils/platform.c \
	cJSON/cJSON.c

# Object files
OBJECTS = $(SOURCES:%.c=$(OBJ_DIR)/%.o)

# Executable name (cross-platform)
TARGET = $(BIN_DIR)/modbus-server$(EXE_EXT)

# Default target
all: $(TARGET)

# Create directories
$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $(OBJ_DIR)/core $(OBJ_DIR)/adapters $(OBJ_DIR)/config $(OBJ_DIR)/json $(OBJ_DIR)/utils
	@mkdir -p $(BIN_DIR)

# Compile object files
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR) $(BIN_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Link executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Clean
clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned build directory"

# Help
help:
	@echo "Makefile targets:"
	@echo "  make          - Build the project"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make help     - Show this help message"

.PHONY: all clean help
