#ifndef SERVER_CONTROLLER_H
#define SERVER_CONTROLLER_H

#include "../config/config.h"
#include "../adapters/modbus_backend.h"
#include "../json/json_command.h"

typedef struct {
    ModbusConfig config;
    ModbusBackend *backend;
    ServerState state;
    bool running;
} ServerController;

/**
 * Create and initialize server controller
 * @param config_file Path to configuration JSON file
 * @return Pointer to ServerController, or NULL on failure
 */
ServerController* server_controller_create(const char *config_file);

/**
 * Destroy server controller and cleanup all resources
 * @param controller Pointer to ServerController
 */
void server_controller_destroy(ServerController *controller);

/**
 * Run the main server loop
 * @param controller Pointer to ServerController
 * @return 0 on normal exit, -1 on error
 */
int server_controller_run(ServerController *controller);

#endif // SERVER_CONTROLLER_H
