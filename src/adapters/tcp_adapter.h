#ifndef TCP_ADAPTER_H
#define TCP_ADAPTER_H

#include "../config/config.h"
#include "modbus_backend.h"
#include <modbus/modbus.h>

/**
 * Initialize TCP Modbus server
 * @param backend Pointer to ModbusBackend
 * @param config Pointer to ModbusConfig
 * @return 0 on success, -1 on failure
 */
int tcp_adapter_init(ModbusBackend *backend, const ModbusConfig *config);

/**
 * Handle TCP client connection
 * @param backend Pointer to ModbusBackend
 * @return 0 on success, -1 on failure
 */
int tcp_adapter_accept_client(ModbusBackend *backend);

/**
 * Handle data from TCP client
 * @param backend Pointer to ModbusBackend
 * @param client_index Index of client in tcp_conn_socks array
 * @param query Buffer for modbus query
 * @return 1 if data received and processed, 0 if no data, -1 if client disconnected
 */
int tcp_adapter_handle_client(ModbusBackend *backend, int client_index, uint8_t *query);

/**
 * Cleanup TCP resources
 * @param backend Pointer to ModbusBackend
 */
void tcp_adapter_cleanup(ModbusBackend *backend);

#endif // TCP_ADAPTER_H
