#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    // Mode settings
    bool enable_tcp;
    bool enable_rtu;
    
    // TCP settings
    int tcp_port;
    
    // RTU settings
    char serial_device[64];
    int baudrate;
    char parity;
    int data_bits;
    int stop_bits;
    
    // Modbus settings
    int unit_id;
    
    // Register mappings
    int coils_start;
    int nb_coils;
    
    int input_bits_start;
    int nb_input_bits;
    
    int holding_regs_start;
    int nb_holding_regs;
    
    int input_regs_start;
    int nb_input_regs;
} ModbusConfig;

/**
 * Load configuration from JSON file
 * @param filename Path to configuration file
 * @param config Pointer to ModbusConfig structure to fill
 * @return 0 on success, -1 on failure
 */
int config_load(const char *filename, ModbusConfig *config);

#endif // CONFIG_H
