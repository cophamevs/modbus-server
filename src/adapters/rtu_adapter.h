#ifndef RTU_ADAPTER_H
#define RTU_ADAPTER_H

#include "../config/config.h"
#include "modbus_backend.h"
#include <modbus/modbus.h>

/**
 * Initialize RTU Modbus slave
 * @param backend Pointer to ModbusBackend
 * @param config Pointer to ModbusConfig
 * @return 0 on success, -1 on failure
 */
int rtu_adapter_init(ModbusBackend *backend, const ModbusConfig *config);

/**
 * Handle data from RTU serial port
 * @param backend Pointer to ModbusBackend
 * @param query Buffer for modbus query
 * @return 1 if data received and processed, 0 if no data, -1 if error
 */
int rtu_adapter_handle(ModbusBackend *backend, uint8_t *query);

/**
 * Try to reconnect RTU connection
 * @param backend Pointer to ModbusBackend
 * @param config Pointer to ModbusConfig
 * @return 0 on success, -1 on failure
 */
int rtu_adapter_reconnect(ModbusBackend *backend, const ModbusConfig *config);

/**
 * Cleanup RTU resources
 * @param backend Pointer to ModbusBackend
 */
void rtu_adapter_cleanup(ModbusBackend *backend);

#endif // RTU_ADAPTER_H
