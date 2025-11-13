#include "rtu_adapter.h"
#include "../utils/logging.h"
#include "../utils/platform.h"
#include <errno.h>

static void set_nonblocking(int fd) {
    if (platform_set_nonblocking(fd) != 0) {
        log_warn("Failed to set non-blocking mode on fd %d", fd);
    }
}

int rtu_adapter_init(ModbusBackend *backend, const ModbusConfig *config) {
    backend->ctx_rtu = modbus_new_rtu(
        config->serial_device,
        config->baudrate,
        config->parity,
        config->data_bits,
        config->stop_bits
    );
    
    if (!backend->ctx_rtu) {
        log_error("Failed to create RTU context");
        return -1;
    }
    
    modbus_set_slave(backend->ctx_rtu, config->unit_id);
    modbus_set_debug(backend->ctx_rtu, FALSE);
    
    // Set non-blocking and timeout
    int rtu_fd = modbus_get_socket(backend->ctx_rtu);
    if (rtu_fd != -1) {
        set_nonblocking(rtu_fd);
        modbus_set_response_timeout(backend->ctx_rtu, 0, 100000); // 100ms
    }
    
    if (modbus_connect(backend->ctx_rtu) == -1) {
        log_error("RTU connect failed: %s", modbus_strerror(errno));
        modbus_free(backend->ctx_rtu);
        backend->ctx_rtu = NULL;
        return -1;
    }
    
    log_debug("RTU connected on %s", config->serial_device);
    return 0;
}

int rtu_adapter_handle(ModbusBackend *backend, uint8_t *query) {
    if (!backend || !backend->ctx_rtu) {
        return -1;
    }
    
    int rc = modbus_receive(backend->ctx_rtu, query);
    if (rc > 0) {
        if (modbus_reply(backend->ctx_rtu, query, rc, backend->mapping) == -1) {
            log_debug("RTU reply failed: %s", modbus_strerror(errno));
            return -1;
        }
        return 1;
    } else if (rc == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            log_debug("RTU error: %s", modbus_strerror(errno));
            return -1;
        }
    }
    
    return 0;
}

int rtu_adapter_reconnect(ModbusBackend *backend, const ModbusConfig *config) {
    if (!backend) return -1;
    
    // Cleanup old connection first
    if (backend->ctx_rtu) {
        modbus_close(backend->ctx_rtu);
        modbus_free(backend->ctx_rtu);
        backend->ctx_rtu = NULL;
    }
    
    log_debug("Attempting to reconnect RTU on %s", config->serial_device);
    
    backend->ctx_rtu = modbus_new_rtu(
        config->serial_device,
        config->baudrate,
        config->parity,
        config->data_bits,
        config->stop_bits
    );
    
    if (!backend->ctx_rtu) {
        log_error("Failed to create RTU context for reconnection");
        return -1;
    }
    
    modbus_set_slave(backend->ctx_rtu, config->unit_id);
    modbus_set_debug(backend->ctx_rtu, FALSE);
    
    int rtu_fd = modbus_get_socket(backend->ctx_rtu);
    if (rtu_fd != -1) {
        set_nonblocking(rtu_fd);
        modbus_set_response_timeout(backend->ctx_rtu, 0, 100000);
    }
    
    if (modbus_connect(backend->ctx_rtu) == -1) {
        log_debug("RTU reconnect failed: %s", modbus_strerror(errno));
        modbus_free(backend->ctx_rtu);
        backend->ctx_rtu = NULL;
        return -1;
    }
    
    log_debug("RTU reconnected successfully on %s", config->serial_device);
    return 0;
}

void rtu_adapter_cleanup(ModbusBackend *backend) {
    if (!backend || !backend->ctx_rtu) {
        return;
    }
    
    modbus_close(backend->ctx_rtu);
    modbus_free(backend->ctx_rtu);
    backend->ctx_rtu = NULL;
}
