#include "config.h"
#include "../utils/logging.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int config_load(const char *filename, ModbusConfig *config) {
    // Set defaults
    config->enable_tcp = true;
    config->enable_rtu = false;
    config->tcp_port = 1502;
    config->unit_id = 1;
    config->coils_start = 0;
    config->nb_coils = 0;
    config->input_bits_start = 0;
    config->nb_input_bits = 0;
    config->holding_regs_start = 0;
    config->nb_holding_regs = 0;
    config->input_regs_start = 0;
    config->nb_input_regs = 0;
    strcpy(config->serial_device, "/dev/ttyUSB0");
    config->baudrate = 9600;
    config->parity = 'N';
    config->data_bits = 8;
    config->stop_bits = 1;
    
    // Read file
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        log_warn("Config file not found: %s, using defaults", filename);
        return 0; // Return 0 to allow running with defaults
    }
    
    char buf[2048];
    size_t len = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[len] = '\0';
    
    // Parse JSON
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        log_error("Failed to parse JSON config");
        return -1;
    }
    
    cJSON *j;
    
    // Parse mode
    if ((j = cJSON_GetObjectItem(root, "mode")) && cJSON_IsString(j)) {
        config->enable_tcp = (strstr(j->valuestring, "tcp") != NULL);
        config->enable_rtu = (strstr(j->valuestring, "rtu") != NULL);
    }
    
    // Parse TCP settings
    if ((j = cJSON_GetObjectItem(root, "tcp_port")) && cJSON_IsNumber(j)) {
        config->tcp_port = j->valueint;
    }
    
    // Parse Modbus settings
    if ((j = cJSON_GetObjectItem(root, "unit_id")) && cJSON_IsNumber(j)) {
        config->unit_id = j->valueint;
    }
    
    // Parse RTU settings
    if ((j = cJSON_GetObjectItem(root, "serial")) && cJSON_IsObject(j)) {
        cJSON *d;
        if ((d = cJSON_GetObjectItem(j, "device")) && cJSON_IsString(d)) {
            strncpy(config->serial_device, d->valuestring, sizeof(config->serial_device) - 1);
        }
        if ((d = cJSON_GetObjectItem(j, "baudrate")) && cJSON_IsNumber(d)) {
            config->baudrate = d->valueint;
        }
        if ((d = cJSON_GetObjectItem(j, "parity")) && cJSON_IsString(d)) {
            config->parity = d->valuestring[0];
        }
        if ((d = cJSON_GetObjectItem(j, "data_bits")) && cJSON_IsNumber(d)) {
            config->data_bits = d->valueint;
        }
        if ((d = cJSON_GetObjectItem(j, "stop_bits")) && cJSON_IsNumber(d)) {
            config->stop_bits = d->valueint;
        }
    }
    
    // Helper macro for parsing register configs
    #define PARSE_REG_CONFIG(name, start_var, count_var) \
    if ((j = cJSON_GetObjectItem(root, name))) { \
        if (cJSON_IsObject(j)) { \
            cJSON *s = cJSON_GetObjectItem(j, "start_address"); \
            cJSON *c = cJSON_GetObjectItem(j, "count"); \
            if (s && c && cJSON_IsNumber(s) && cJSON_IsNumber(c)) { \
                start_var = s->valueint; \
                count_var = c->valueint; \
            } \
        } else if (cJSON_IsArray(j) && j->child && j->child->next) { \
            start_var = j->child->valueint; \
            count_var = j->child->next->valueint; \
        } \
    }
    
    PARSE_REG_CONFIG("coils", config->coils_start, config->nb_coils);
    PARSE_REG_CONFIG("input_bits", config->input_bits_start, config->nb_input_bits);
    PARSE_REG_CONFIG("holding_registers", config->holding_regs_start, config->nb_holding_regs);
    PARSE_REG_CONFIG("input_registers", config->input_regs_start, config->nb_input_regs);
    
    cJSON_Delete(root);
    log_debug("Config loaded successfully");
    return 0;
}
