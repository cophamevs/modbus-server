#include "tcp_adapter.h"
#include "../utils/logging.h"
#include "../utils/platform.h"
#include <stdlib.h>
#include <string.h>

int tcp_adapter_init(ModbusBackend *backend, const ModbusConfig *config) {
    backend->ctx_tcp = modbus_new_tcp(NULL, config->tcp_port);
    if (!backend->ctx_tcp) {
        log_error("Failed to create TCP context");
        return -1;
    }
    
    modbus_set_slave(backend->ctx_tcp, config->unit_id);
    modbus_set_debug(backend->ctx_tcp, FALSE);
    
    backend->tcp_listen_sock = modbus_tcp_listen(backend->ctx_tcp, 1);
    if (backend->tcp_listen_sock == -1) {
        log_error("TCP listen failed");
        modbus_free(backend->ctx_tcp);
        backend->ctx_tcp = NULL;
        return -1;
    }
    
    // Initialize TCP client array
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
        backend->tcp_conn_socks[i] = -1;
    }
    backend->tcp_conn_count = 0;
    
    log_debug("TCP Server listening on port %d", config->tcp_port);
    return 0;
}

static int find_free_tcp_client_slot(ModbusBackend *backend) {
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
        if (backend->tcp_conn_socks[i] == -1) {
            return i;
        }
    }
    return -1;
}

int tcp_adapter_accept_client(ModbusBackend *backend) {
    int slot = find_free_tcp_client_slot(backend);
    if (slot == -1) {
        log_warn("Max TCP clients reached (%d), rejecting connection", MAX_TCP_CLIENTS);
        int temp_sock = modbus_tcp_accept(backend->ctx_tcp, &backend->tcp_listen_sock);
        if (temp_sock != -1) platform_close_fd(temp_sock);
        return -1;
    }
    
    int new_conn = modbus_tcp_accept(backend->ctx_tcp, &backend->tcp_listen_sock);
    if (new_conn == -1) {
        return -1;
    }
    
    backend->tcp_conn_socks[slot] = new_conn;
    backend->tcp_conn_count++;
    log_debug("TCP client connected (slot %d, fd %d), total clients: %d", 
              slot, new_conn, backend->tcp_conn_count);
    return slot;
}

int tcp_adapter_handle_client(ModbusBackend *backend, int client_index, uint8_t *query) {
    if (client_index < 0 || client_index >= MAX_TCP_CLIENTS) {
        return -1;
    }
    
    int sock = backend->tcp_conn_socks[client_index];
    if (sock == -1) {
        return -1;
    }
    
    modbus_set_socket(backend->ctx_tcp, sock);
    int rc = modbus_receive(backend->ctx_tcp, query);
    
    if (rc > 0) {
        if (modbus_reply(backend->ctx_tcp, query, rc, backend->mapping) == -1) {
            log_debug("TCP reply failed for client %d: %s", client_index, modbus_strerror(errno));
            return -1;
        }
        return 1;
    } else if (rc == -1) {
        log_debug("TCP client disconnect (slot %d)", client_index);
        platform_close_fd(sock);
        backend->tcp_conn_socks[client_index] = -1;
        backend->tcp_conn_count--;
        return -1;
    }
    
    return 0;
}

void tcp_adapter_cleanup(ModbusBackend *backend) {
    if (!backend) return;
    
    // Close all client connections
    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
        if (backend->tcp_conn_socks[i] != -1) {
            platform_close_fd(backend->tcp_conn_socks[i]);
            backend->tcp_conn_socks[i] = -1;
        }
    }
    backend->tcp_conn_count = 0;
    
    if (backend->tcp_listen_sock != -1) {
        platform_close_fd(backend->tcp_listen_sock);
        backend->tcp_listen_sock = -1;
    }
    
    if (backend->ctx_tcp) {
        modbus_free(backend->ctx_tcp);
        backend->ctx_tcp = NULL;
    }
}
