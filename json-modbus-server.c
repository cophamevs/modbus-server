#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include "cJSON/cJSON.h"

#define MAX_JSON_BUFFER 512
#define log_debug(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)

typedef enum { STATE_STOPPED, STATE_RUNNING } ServerState;
volatile ServerState server_state = STATE_RUNNING;

modbus_mapping_t *mb_mapping;
int coils_start=0, nb_coils=0;
int input_bits_start=0, nb_input_bits=0;
int holding_regs_start=0, nb_holding_regs=0;
int input_regs_start=0, nb_input_regs=0;
int unit_id=1, tcp_port=1502;
char mode[8]="tcp";
char serial_device[64]="/dev/ttyUSB0";
int baudrate=9600;
char parity='N';
int data_bits=8;
int stop_bits=1;

// Byte order for multi-register values
typedef enum { ORDER_LE, ORDER_BE, ORDER_SWAP } ByteOrder;

static int load_config(const char *file) {
    FILE *fp = fopen(file, "r"); if (!fp) return -1;
    char buf[2048]; size_t len = fread(buf,1,sizeof(buf)-1,fp); buf[len]='\0'; fclose(fp);
    cJSON *root = cJSON_Parse(buf); if(!root) return -1;
    cJSON *j;
    if((j=cJSON_GetObjectItem(root,"mode"))&&cJSON_IsString(j)) strncpy(mode,j->valuestring,sizeof(mode)-1);
    tcp_port = cJSON_GetObjectItem(root,"tcp_port")->valueint;
    unit_id  = cJSON_GetObjectItem(root,"unit_id")->valueint;
    if((j=cJSON_GetObjectItem(root,"serial"))){
        cJSON *d;
        if((d=cJSON_GetObjectItem(j,"device"))&&cJSON_IsString(d)) strncpy(serial_device,d->valuestring,sizeof(serial_device)-1);
        baudrate  = cJSON_GetObjectItem(j,"baudrate")->valueint;
        parity    = cJSON_GetObjectItem(j,"parity")->valuestring[0];
        data_bits = cJSON_GetObjectItem(j,"data_bits")->valueint;
        stop_bits = cJSON_GetObjectItem(j,"stop_bits")->valueint;
    }
    if((j=cJSON_GetObjectItem(root,"coils"))) { coils_start=j->child->valueint; nb_coils=j->child->next->valueint; }
    if((j=cJSON_GetObjectItem(root,"input_bits"))) { input_bits_start=j->child->valueint; nb_input_bits=j->child->next->valueint; }
    if((j=cJSON_GetObjectItem(root,"holding_registers"))) { holding_regs_start=j->child->valueint; nb_holding_regs=j->child->next->valueint; }
    if((j=cJSON_GetObjectItem(root,"input_registers")))  { input_regs_start=j->child->valueint; nb_input_regs=j->child->next->valueint; }
    cJSON_Delete(root);
    return 0;
}

// Parse byte order string
static ByteOrder parse_byte_order(const char *s) {
    if (!s || strcasecmp(s, "LE") == 0) return ORDER_LE;
    if (strcasecmp(s, "BE") == 0) return ORDER_BE;
    if (strcasecmp(s, "SWAP") == 0) return ORDER_SWAP;
    return ORDER_LE;
}

// Write multi-word data into registers with correct byte order
static void write_registers(int start_idx, uint16_t *words, int count, ByteOrder order) {
    // Swap bytes within words if SWAP
    if(order == ORDER_SWAP) {
        for(int i=0;i<count;i++) {
            words[i] = (words[i] >> 8) | (words[i] << 8);
        }
    }
    // Swap word order if BE (for 2-word data)
    if(order == ORDER_BE && count == 2) {
        uint16_t tmp = words[0]; words[0] = words[1]; words[1] = tmp;
    }
    for(int i=0;i<count;i++) {
        mb_mapping->tab_registers[start_idx + i] = words[i];
    }
}

static void process_json(const char *json_str, modbus_t *ctx, int *listen_sock, int *conn_sock) {
    log_debug("Received JSON: %s", json_str);
    cJSON *root = cJSON_Parse(json_str); if(!root) return;
    cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root,"cmd");
    if(cJSON_IsString(cmd)){
        if(strcmp(cmd->valuestring,"stop")==0){
            server_state = STATE_STOPPED;
            printf("{\"status\":\"stopped\"}\n");
            if(*conn_sock!=-1) { close(*conn_sock); *conn_sock=-1; }
            cJSON_Delete(root);
            return;
        }
        if(strcmp(cmd->valuestring,"start")==0){
            server_state = STATE_RUNNING;
            printf("{\"status\":\"running\"}\n");
            cJSON_Delete(root);
            return;
        }
    }
    // Data update
    cJSON *type     = cJSON_GetObjectItemCaseSensitive(root,"type");
    cJSON *addr     = cJSON_GetObjectItemCaseSensitive(root,"address");
    cJSON *datatype = cJSON_GetObjectItemCaseSensitive(root,"datatype");
    cJSON *order_it = cJSON_GetObjectItemCaseSensitive(root,"byte_order");
    cJSON *val      = cJSON_GetObjectItemCaseSensitive(root,"value");
    if(!cJSON_IsString(type)||!cJSON_IsNumber(addr)||!cJSON_IsString(datatype)||!val) { cJSON_Delete(root); return; }
    int idx = addr->valueint - holding_regs_start;
    ByteOrder bo = parse_byte_order(order_it ? order_it->valuestring : "LE");
    // Handle types
    if(strcmp(datatype->valuestring,"uint16")==0 && cJSON_IsNumber(val)) {
        uint16_t v = (uint16_t)val->valueint;
        if(bo == ORDER_SWAP) v = (v>>8)|(v<<8);
        mb_mapping->tab_registers[idx] = v;
    }
    else if(strcmp(datatype->valuestring,"int16")==0 && cJSON_IsNumber(val)) {
        int16_t iv = (int16_t)val->valueint;
        uint16_t v = *(uint16_t *)&iv;
        if(bo == ORDER_SWAP) v = (v>>8)|(v<<8);
        mb_mapping->tab_registers[idx] = v;
    }
    else if((strcmp(datatype->valuestring,"uint32")==0 || strcmp(datatype->valuestring,"int32")==0) && cJSON_IsNumber(val)) {
        uint32_t u = strcmp(datatype->valuestring,"int32")==0 ? (uint32_t)(int32_t)val->valueint : (uint32_t)val->valueint;
        uint16_t words[2] = { (uint16_t)(u & 0xFFFF), (uint16_t)(u >> 16) };
        write_registers(idx, words, 2, bo);
    }
    else if(strcmp(datatype->valuestring,"float")==0 && cJSON_IsNumber(val)) {
        union { float f; uint32_t u32; } fu; fu.f = (float)val->valuedouble;
        uint16_t words[2] = { (uint16_t)(fu.u32 & 0xFFFF), (uint16_t)(fu.u32 >> 16) };
        write_registers(idx, words, 2, bo);
    }
    cJSON_Delete(root);
}

int main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);
    if(load_config("/home/pi/meter/json-modbus-server-c/modbus_config.json")!=0) {
        fprintf(stderr,"Config load failed\n"); exit(EXIT_FAILURE);
    }
    modbus_t *ctx;
    int listen_sock=-1, conn_sock=-1;
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    if(strcmp(mode,"rtu")==0) {
        ctx = modbus_new_rtu(serial_device,baudrate,parity,data_bits,stop_bits);
        modbus_set_slave(ctx,unit_id);
        if(modbus_connect(ctx)==-1) { fprintf(stderr,"RTU connect failed: %s\n",modbus_strerror(errno)); return -1; }
        printf("{\"status\":\"server_ready\",\"mode\":\"%s\",\"device\":\"%s\",\"baudrate\":%d,\"parity\":\"%c\",\"data_bits\":%d,\"stop_bits\":%d,\"unit_id\":%d}\n",
               mode, serial_device, baudrate, parity, data_bits, stop_bits, unit_id);
    } else {
        ctx = modbus_new_tcp(NULL,tcp_port);
        modbus_set_slave(ctx,unit_id);
        listen_sock = modbus_tcp_listen(ctx,1);
        printf("{\"status\":\"server_ready\",\"mode\":\"%s\",\"port\":%d,\"unit_id\":%d}\n",mode,tcp_port,unit_id);
    }
    modbus_set_debug(ctx,FALSE);
    mb_mapping = modbus_mapping_new_start_address(
        coils_start,nb_coils,
        input_bits_start,nb_input_bits,
        holding_regs_start,nb_holding_regs,
        input_regs_start,nb_input_regs);
    if(!mb_mapping){ fprintf(stderr,"Mapping alloc failed\n"); modbus_free(ctx); return -1; }
    while(1) {
        fd_set fds; FD_ZERO(&fds);
        if(listen_sock!=-1) FD_SET(listen_sock,&fds);
        if(conn_sock  !=-1) FD_SET(conn_sock,&fds);
        FD_SET(STDIN_FILENO,&fds);
        int maxfd = STDIN_FILENO;
        if(listen_sock>maxfd) maxfd=listen_sock;
        if(conn_sock>maxfd)   maxfd=conn_sock;
        if(select(maxfd+1,&fds,NULL,NULL,NULL)>0) {
            if(FD_ISSET(STDIN_FILENO,&fds)) {
                char buf[MAX_JSON_BUFFER]; if(fgets(buf,sizeof(buf),stdin)) process_json(buf,ctx,&listen_sock,&conn_sock);
            }
            if(server_state==STATE_RUNNING) {
                if(listen_sock!=-1 && FD_ISSET(listen_sock,&fds)) {
                    conn_sock = modbus_tcp_accept(ctx,&listen_sock);
                    modbus_set_socket(ctx,conn_sock);
                    log_debug("New client: %d", conn_sock);
                }
                if(strcmp(mode,"rtu")==0) {
                    int rc = modbus_receive(ctx,query);
                    if(rc>0) modbus_reply(ctx,query,rc,mb_mapping);
                } else if(conn_sock!=-1 && FD_ISSET(conn_sock,&fds)) {
                    int rc = modbus_receive(ctx,query);
                    if(rc>0) modbus_reply(ctx,query,rc,mb_mapping);
                }
            }
        }
    }
    if(conn_sock   != -1) close(conn_sock);
    if(listen_sock != -1) close(listen_sock);
    modbus_mapping_free(mb_mapping);
    modbus_close(ctx);
    modbus_free(ctx);
    return 0;
}
