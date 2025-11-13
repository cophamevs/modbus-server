#include "json_command.h"
#include "../utils/logging.h"
#include "../utils/byte_order.h"
#include "cJSON/cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

void json_command_process(
    const char *json_str,
    ModbusBackend *backend,
    ServerState *state,
    bool *running) {
    
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        log_warn("Failed to parse JSON command");
        return;
    }
    
    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    
    if (cJSON_IsString(cmd)) {
        if (strcmp(cmd->valuestring, "stop") == 0) {
            *state = STATE_STOPPED;
            *running = false;
            printf("{\"status\":\"stopping\"}\n");
            cJSON_Delete(root);
            return;
        }
        
        if (strcmp(cmd->valuestring, "start") == 0) {
            if (*state == STATE_RUNNING) {
                printf("{\"error\":\"already_running\"}\n");
                cJSON_Delete(root);
                return;
            }
            *state = STATE_RUNNING;
            printf("{\"status\":\"starting\"}\n");
            cJSON_Delete(root);
            return;
        }
        
        if (strcmp(cmd->valuestring, "status") == 0) {
            printf("{\"status\":\"%s\"}\n", *state == STATE_RUNNING ? "running" : "stopped");
            cJSON_Delete(root);
            return;
        }
    }
    
    // Try to process as data update
    json_command_update_data(json_str, backend, NULL);
    cJSON_Delete(root);
}

int json_command_update_data(
    const char *json_str,
    ModbusBackend *backend,
    const ModbusConfig *config) {
    
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return -1;
    }
    
    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON *addr = cJSON_GetObjectItemCaseSensitive(root, "address");
    cJSON *datatype = cJSON_GetObjectItemCaseSensitive(root, "datatype");
    cJSON *order_it = cJSON_GetObjectItemCaseSensitive(root, "byte_order");
    cJSON *val = cJSON_GetObjectItemCaseSensitive(root, "value");
    
    if (!cJSON_IsString(type) || !cJSON_IsNumber(addr) || 
        !cJSON_IsString(datatype) || !val) {
        cJSON_Delete(root);
        return -1;
    }
    
    if (!backend || !backend->mapping) {
        cJSON_Delete(root);
        return -1;
    }
    
    int addr_val = addr->valueint;
    int idx = addr_val;
    
    if (idx < 0 || idx >= backend->mapping->nb_registers) {
        printf("{\"error\":\"index_out_of_bounds\",\"address\":%d}\n", addr_val);
        cJSON_Delete(root);
        return -1;
    }
    
    ByteOrder bo = parse_byte_order(order_it ? order_it->valuestring : "LE");
    
    // Handle data types
    if (strcmp(datatype->valuestring, "uint16") == 0 && cJSON_IsNumber(val)) {
        uint16_t v = (uint16_t)val->valueint;
        if (bo == ORDER_SWAP) v = (v >> 8) | (v << 8);
        backend->mapping->tab_registers[idx] = v;
    }
    else if (strcmp(datatype->valuestring, "int16") == 0 && cJSON_IsNumber(val)) {
        int16_t iv = (int16_t)val->valueint;
        uint16_t v = *(uint16_t *)&iv;
        if (bo == ORDER_SWAP) v = (v >> 8) | (v << 8);
        backend->mapping->tab_registers[idx] = v;
    }
    else if ((strcmp(datatype->valuestring, "uint32") == 0 || 
              strcmp(datatype->valuestring, "int32") == 0) && cJSON_IsNumber(val)) {
        uint32_t u = strcmp(datatype->valuestring, "int32") == 0 ?
                     (uint32_t)(int32_t)val->valueint : (uint32_t)val->valueint;
        uint16_t words[2] = { (uint16_t)(u & 0xFFFF), (uint16_t)(u >> 16) };
        write_registers(idx, words, 2, bo, backend->mapping->tab_registers);
    }
    else if (strcmp(datatype->valuestring, "float") == 0 && cJSON_IsNumber(val)) {
        union { float f; uint32_t u32; } fu;
        fu.f = (float)val->valuedouble;
        uint16_t words[2] = { (uint16_t)(fu.u32 & 0xFFFF), (uint16_t)(fu.u32 >> 16) };
        write_registers(idx, words, 2, bo, backend->mapping->tab_registers);
    }
    
    printf("{\"status\":\"updated\",\"address\":%d,\"datatype\":\"%s\"}\n", addr_val, datatype->valuestring);
    cJSON_Delete(root);
    return 0;
}
