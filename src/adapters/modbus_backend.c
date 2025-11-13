#include "modbus_backend.h"
#include "../utils/logging.h"
#include <stdlib.h>

ModbusBackend* modbus_backend_create(void) {
    ModbusBackend *backend = (ModbusBackend *)malloc(sizeof(ModbusBackend));
    if (!backend) {
        log_error("Failed to allocate ModbusBackend");
        return NULL;
    }
    
    // Initialize all fields to NULL/0
    backend->ctx_tcp = NULL;
    backend->ctx_rtu = NULL;
    backend->mapping = NULL;
    backend->tcp_listen_sock = -1;
    backend->tcp_conn_count = 0;
    
    // Initialize TCP client array
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
        backend->tcp_conn_socks[i] = -1;
    }
    
    log_debug("ModbusBackend created successfully");
    return backend;
}

void modbus_backend_destroy(ModbusBackend *backend) {
    if (!backend) return;
    
    // Cleanup will be done by adapters
    free(backend);
    log_debug("ModbusBackend destroyed");
}
