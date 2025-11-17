#include "core/server_controller.h"
#include "utils/logging.h"
#include "utils/platform.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
    // Initialize platform
    if (platform_init() != 0) {
        log_error("Failed to initialize platform");
        return EXIT_FAILURE;
    }
    
    const char *config_file = (argc > 1) ? argv[1] : "modbus_config.json";
    
    log_info("Starting Modbus JSON Server with config: %s", config_file);
    
    ServerController *controller = server_controller_create(config_file);
    if (!controller) {
        log_error("Failed to create server controller");
        platform_cleanup();
        return EXIT_FAILURE;
    }
    
    // Start server automatically
    #ifdef _WIN32
    controller->state = STATE_RUNNING;
    #else
    controller->state = STATE_RUNNING;
    #endif
    
    int ret = server_controller_run(controller);
    
    server_controller_destroy(controller);
    platform_cleanup();
    
    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
