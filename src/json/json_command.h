#ifndef JSON_COMMAND_H
#define JSON_COMMAND_H

#include "../config/config.h"
#include "../adapters/modbus_backend.h"
#include <stdbool.h>

typedef enum {
    STATE_STOPPED,
    STATE_RUNNING
} ServerState;

/**
 * Process JSON command from stdin
 * @param json_str JSON string to parse
 * @param backend Pointer to ModbusBackend
 * @param state Pointer to current server state
 * @param running Pointer to running flag
 */
void json_command_process(
    const char *json_str,
    ModbusBackend *backend,
    ServerState *state,
    bool *running
);

/**
 * Process JSON data update command
 * @param json_str JSON string containing data update
 * @param backend Pointer to ModbusBackend
 * @param config Pointer to ModbusConfig
 * @return 0 on success, -1 on failure
 */
int json_command_update_data(
    const char *json_str,
    ModbusBackend *backend,
    const ModbusConfig *config
);

#endif // JSON_COMMAND_H
