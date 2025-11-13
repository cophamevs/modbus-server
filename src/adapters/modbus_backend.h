#ifndef MODBUS_BACKEND_H
#define MODBUS_BACKEND_H

#include <modbus/modbus.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    modbus_t *ctx_tcp;
    modbus_t *ctx_rtu;
    modbus_mapping_t *mapping;
    
    int tcp_listen_sock;
    
    // TCP client management
    #define MAX_TCP_CLIENTS 10
    int tcp_conn_socks[MAX_TCP_CLIENTS];
    int tcp_conn_count;
} ModbusBackend;

/**
 * Initialize Modbus backend
 * @return Pointer to ModbusBackend structure, or NULL on failure
 */
ModbusBackend* modbus_backend_create(void);

/**
 * Cleanup Modbus backend
 * @param backend Pointer to ModbusBackend structure
 */
void modbus_backend_destroy(ModbusBackend *backend);

#endif // MODBUS_BACKEND_H
