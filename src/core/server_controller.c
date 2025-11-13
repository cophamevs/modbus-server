#include "server_controller.h"
#include "../utils/logging.h"
#include "../utils/platform.h"
#include "../adapters/tcp_adapter.h"
#include "../adapters/rtu_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include "cJSON/cJSON.h"

#ifdef _WIN32
#include <winsock2.h>
#define MSG_DONTWAIT 0
#define recv(s, b, l, f) recvfrom(s, b, l, f, NULL, NULL)
#else
#include <sys/socket.h>
#endif

static ServerController *g_controller = NULL;

static void signal_handler(int sig) {
#ifdef _WIN32
    if (sig == SIGINT) {
        if (g_controller) {
            g_controller->state = STATE_STOPPED;
            g_controller->running = false;
            log_debug("Received signal %d, shutting down", sig);
        }
    }
#else
    if (sig == SIGTERM || sig == SIGINT) {
        if (g_controller) {
            g_controller->state = STATE_STOPPED;
            g_controller->running = false;
            log_debug("Received signal %d, shutting down", sig);
        }
    }
#endif
}

ServerController* server_controller_create(const char *config_file) {
    ServerController *controller = (ServerController *)malloc(sizeof(ServerController));
    if (!controller) {
        log_error("Failed to allocate ServerController");
        return NULL;
    }
    
    // Load configuration
    if (config_load(config_file, &controller->config) != 0) {
        log_error("Failed to load configuration from %s", config_file);
        free(controller);
        return NULL;
    }
    
    // Create backend
    controller->backend = modbus_backend_create();
    if (!controller->backend) {
        log_error("Failed to create Modbus backend");
        free(controller);
        return NULL;
    }
    
    // Initialize state
    controller->state = STATE_STOPPED;
    controller->running = true;
    
    // Setup signal handlers
    g_controller = controller;
#ifdef _WIN32
    signal(SIGINT, signal_handler);
#else
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
#endif
    
    log_debug("ServerController created successfully");
    return controller;
}

void server_controller_destroy(ServerController *controller) {
    if (!controller) return;
    
    if (controller->backend) {
        // Cleanup adapters
        tcp_adapter_cleanup(controller->backend);
        rtu_adapter_cleanup(controller->backend);
        
        // Free mapping
        if (controller->backend->mapping) {
            modbus_mapping_free(controller->backend->mapping);
            controller->backend->mapping = NULL;
        }
        
        free(controller->backend);
        controller->backend = NULL;
    }
    
    free(controller);
    g_controller = NULL;
    log_debug("ServerController destroyed");
}

static int initialize_server(ServerController *controller) {
    // Create modbus mapping
    controller->backend->mapping = modbus_mapping_new_start_address(
        controller->config.coils_start, controller->config.nb_coils,
        controller->config.input_bits_start, controller->config.nb_input_bits,
        controller->config.holding_regs_start, controller->config.nb_holding_regs,
        controller->config.input_regs_start, controller->config.nb_input_regs
    );
    
    if (!controller->backend->mapping) {
        log_error("Failed to allocate modbus mapping");
        return -1;
    }
    
    // Initialize TCP adapter if enabled
    if (controller->config.enable_tcp) {
        if (tcp_adapter_init(controller->backend, &controller->config) != 0) {
            log_error("Failed to initialize TCP adapter");
            modbus_mapping_free(controller->backend->mapping);
            controller->backend->mapping = NULL;
            return -1;
        }
    }
    
    // Initialize RTU adapter if enabled
    if (controller->config.enable_rtu) {
        if (rtu_adapter_init(controller->backend, &controller->config) != 0) {
            log_warn("Failed to initialize RTU adapter (will attempt reconnection)");
        }
    }
    
    printf("{\"status\":\"server_ready\",\"tcp\":%s,\"rtu\":%s,\"unit_id\":%d}\n",
           controller->config.enable_tcp ? "true" : "false",
           controller->config.enable_rtu ? "true" : "false",
           controller->config.unit_id);
    fflush(stdout);
    
    return 0;
}

static void shutdown_server(ServerController *controller) {
    if (!controller->backend || !controller->backend->mapping) {
        return;
    }
    
    tcp_adapter_cleanup(controller->backend);
    rtu_adapter_cleanup(controller->backend);
    
    modbus_mapping_free(controller->backend->mapping);
    controller->backend->mapping = NULL;
    
    printf("{\"status\":\"server_stopped\"}\n");
    fflush(stdout);
}

int server_controller_run(ServerController *controller) {
    if (!controller) {
        return -1;
    }
    
    setvbuf(stdout, NULL, _IOLBF, 0);
    
    printf("{\"ready\":true}\n");
    fflush(stdout);
    
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    char buf[512];
    
    while (controller->running) {
#ifdef _WIN32
        // Windows: Use simple loop with TCP handling
        platform_msleep(50);
        
        // Check stdin for commands
        if (platform_read_stdin(buf, sizeof(buf), 0) > 0) {
            json_command_process(buf, controller->backend, &controller->state, &controller->running);
        }
        
        // Initialize server on first STATE_RUNNING
        if (controller->state == STATE_RUNNING && !controller->backend->mapping) {
            if (initialize_server(controller) != 0) {
                controller->state = STATE_STOPPED;
                continue;
            }
        }
        
        // Cleanup when stopping
        if (controller->state == STATE_STOPPED && controller->backend->mapping) {
            shutdown_server(controller);
        }
        
        // Handle TCP connections when running
        if (controller->state == STATE_RUNNING && controller->backend->mapping) {
            // Accept new TCP connections (non-blocking check)
            if (controller->config.enable_tcp && controller->backend->tcp_listen_sock != -1) {
                // Try to accept with short timeout
                struct sockaddr_in client_addr;
                int client_addr_len = sizeof(client_addr);
                
                int new_conn = accept(controller->backend->tcp_listen_sock, 
                                     (struct sockaddr *)&client_addr, 
                                     &client_addr_len);
                if (new_conn != -1) {
                    int slot = -1;
                    for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
                        if (controller->backend->tcp_conn_socks[i] == -1) {
                            slot = i;
                            break;
                        }
                    }
                    if (slot != -1) {
                        controller->backend->tcp_conn_socks[slot] = new_conn;
                        controller->backend->tcp_conn_count++;
                        log_debug("TCP client connected (slot %d, fd %d)", slot, new_conn);
                    } else {
                        closesocket(new_conn);
                        log_warn("Max TCP clients reached");
                    }
                }
            }
            
            // Handle all active TCP clients
            if (controller->config.enable_tcp) {
                for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
                    int sock = controller->backend->tcp_conn_socks[i];
                    if (sock != -1) {
                        // Read JSON command from TCP socket
                        int n = recv(sock, (char *)query, sizeof(query) - 1, MSG_DONTWAIT);
                        if (n > 0) {
                            query[n] = '\0';
                            log_debug("TCP received (%d bytes): %s", n, (char *)query);
                            
                            // Process JSON command and send response
                            char response_buf[512];
                            int response_len = 0;
                            
                            cJSON *root = cJSON_Parse((const char *)query);
                            if (root) {
                                cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
                                if (cJSON_IsString(cmd)) {
                                    if (strcmp(cmd->valuestring, "status") == 0) {
                                        response_len = snprintf(response_buf, sizeof(response_buf),
                                            "{\"status\":\"%s\"}\n",
                                            controller->state == STATE_RUNNING ? "running" : "stopped");
                                    } else if (strcmp(cmd->valuestring, "start") == 0) {
                                        if (controller->state == STATE_RUNNING) {
                                            response_len = snprintf(response_buf, sizeof(response_buf), "{\"error\":\"already_running\"}\n");
                                        } else {
                                            controller->state = STATE_RUNNING;
                                            response_len = snprintf(response_buf, sizeof(response_buf), "{\"status\":\"starting\"}\n");
                                        }
                                    } else if (strcmp(cmd->valuestring, "stop") == 0) {
                                        controller->state = STATE_STOPPED;
                                        controller->running = false;
                                        response_len = snprintf(response_buf, sizeof(response_buf), "{\"status\":\"stopping\"}\n");
                                    }
                                } else {
                                    // Try to process as data update command
                                    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
                                    cJSON *addr = cJSON_GetObjectItemCaseSensitive(root, "address");
                                    cJSON *datatype = cJSON_GetObjectItemCaseSensitive(root, "datatype");
                                    cJSON *val = cJSON_GetObjectItemCaseSensitive(root, "value");
                                    
                                    if (cJSON_IsString(type) && cJSON_IsNumber(addr) && cJSON_IsString(datatype) && val) {
                                        if (!controller->backend->mapping) {
                                            response_len = snprintf(response_buf, sizeof(response_buf), "{\"error\":\"server_not_running\"}\n");
                                        } else {
                                            int addr_val = addr->valueint;
                                            int idx = addr_val;
                                            
                                            if (idx < 0 || idx >= controller->backend->mapping->nb_registers) {
                                                response_len = snprintf(response_buf, sizeof(response_buf), 
                                                    "{\"error\":\"index_out_of_bounds\",\"address\":%d}\n", addr_val);
                                            } else {
                                                // Update register
                                                if (strcmp(datatype->valuestring, "uint16") == 0 && cJSON_IsNumber(val)) {
                                                    uint16_t v = (uint16_t)val->valueint;
                                                    controller->backend->mapping->tab_registers[idx] = v;
                                                    response_len = snprintf(response_buf, sizeof(response_buf),
                                                        "{\"status\":\"updated\",\"address\":%d,\"datatype\":\"uint16\",\"value\":%d}\n",
                                                        addr_val, v);
                                                } else if (strcmp(datatype->valuestring, "int16") == 0 && cJSON_IsNumber(val)) {
                                                    int16_t iv = (int16_t)val->valueint;
                                                    uint16_t v = *(uint16_t *)&iv;
                                                    controller->backend->mapping->tab_registers[idx] = v;
                                                    response_len = snprintf(response_buf, sizeof(response_buf),
                                                        "{\"status\":\"updated\",\"address\":%d,\"datatype\":\"int16\"}\n",
                                                        addr_val);
                                                } else if (strcmp(datatype->valuestring, "uint32") == 0 && cJSON_IsNumber(val)) {
                                                    uint32_t u = (uint32_t)val->valueint;
                                                    if (idx + 1 < controller->backend->mapping->nb_registers) {
                                                        controller->backend->mapping->tab_registers[idx] = (uint16_t)(u & 0xFFFF);
                                                        controller->backend->mapping->tab_registers[idx + 1] = (uint16_t)(u >> 16);
                                                        response_len = snprintf(response_buf, sizeof(response_buf),
                                                            "{\"status\":\"updated\",\"address\":%d,\"datatype\":\"uint32\",\"value\":%u}\n",
                                                            addr_val, u);
                                                    } else {
                                                        response_len = snprintf(response_buf, sizeof(response_buf),
                                                            "{\"error\":\"insufficient_space\",\"address\":%d}\n", addr_val);
                                                    }
                                                } else {
                                                    response_len = snprintf(response_buf, sizeof(response_buf),
                                                        "{\"error\":\"unsupported_datatype\"}\n");
                                                }
                                            }
                                        }
                                    } else {
                                        response_len = snprintf(response_buf, sizeof(response_buf), "{\"error\":\"invalid_command\"}\n");
                                    }
                                }
                                cJSON_Delete(root);
                            } else {
                                response_len = snprintf(response_buf, sizeof(response_buf), "{\"error\":\"invalid_json\"}\n");
                            }
                            
                            // Send response back
                            if (response_len > 0) {
                                send(sock, response_buf, response_len, 0);
                            }
                        } else if (n < 0) {
                            // Error or no data
                            #ifdef _WIN32
                            int err = WSAGetLastError();
                            if (err != WSAEWOULDBLOCK && err != WSAEINTR) {
                                log_debug("TCP client error (slot %d): %d", i, err);
                                closesocket(sock);
                                controller->backend->tcp_conn_socks[i] = -1;
                                controller->backend->tcp_conn_count--;
                            }
                            #else
                            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                                log_debug("TCP client disconnect (slot %d)", i);
                                close(sock);
                                controller->backend->tcp_conn_socks[i] = -1;
                                controller->backend->tcp_conn_count--;
                            }
                            #endif
                        } else {
                            // n == 0: connection closed
                            log_debug("TCP client disconnect (slot %d)", i);
                            closesocket(sock);
                            controller->backend->tcp_conn_socks[i] = -1;
                            controller->backend->tcp_conn_count--;
                        }
                    }
                }
            }
            
            // Handle RTU
            if (controller->config.enable_rtu && controller->backend->ctx_rtu) {
                int rc = modbus_receive(controller->backend->ctx_rtu, query);
                if (rc > 0) {
                    if (modbus_reply(controller->backend->ctx_rtu, query, rc, controller->backend->mapping) == -1) {
                        log_debug("RTU reply failed");
                    }
                }
            }
        }
        
#else
        // Linux/Unix: Use select() for efficient multiplexing
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        
        int maxfd = STDIN_FILENO;
        
        // Add server sockets only if running
        if (controller->state == STATE_RUNNING) {
            if (controller->config.enable_tcp && controller->backend->tcp_listen_sock != -1) {
                FD_SET(controller->backend->tcp_listen_sock, &fds);
                if (controller->backend->tcp_listen_sock > maxfd) {
                    maxfd = controller->backend->tcp_listen_sock;
                }
            }
            
            // Add TCP client sockets
            if (controller->config.enable_tcp) {
                for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
                    if (controller->backend->tcp_conn_socks[i] != -1) {
                        FD_SET(controller->backend->tcp_conn_socks[i], &fds);
                        if (controller->backend->tcp_conn_socks[i] > maxfd) {
                            maxfd = controller->backend->tcp_conn_socks[i];
                        }
                    }
                }
            }
            
            // Add RTU socket
            if (controller->config.enable_rtu && controller->backend->ctx_rtu) {
                int rtu_fd = modbus_get_socket(controller->backend->ctx_rtu);
                if (rtu_fd != -1) {
                    FD_SET(rtu_fd, &fds);
                    if (rtu_fd > maxfd) maxfd = rtu_fd;
                }
            }
        }
        
        struct timeval tv = {.tv_sec = 0, .tv_usec = 100000};
        int ret = select(maxfd + 1, &fds, NULL, NULL, &tv);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            log_error("select() failed: %s", strerror(errno));
            break;
        }
        
        // Process stdin commands
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            if (fgets(buf, sizeof(buf), stdin)) {
                json_command_process(buf, controller->backend, &controller->state, &controller->running);
            } else {
                controller->running = false;
                controller->state = STATE_STOPPED;
            }
        }
        
        // Initialize server on first STATE_RUNNING
        if (controller->state == STATE_RUNNING && !controller->backend->mapping) {
            if (initialize_server(controller) != 0) {
                controller->state = STATE_STOPPED;
                continue;
            }
        }
        
        // Cleanup when stopping
        if (controller->state == STATE_STOPPED && controller->backend->mapping) {
            shutdown_server(controller);
        }
        
        // Handle connections only when running
        if (controller->state == STATE_RUNNING && ret > 0) {
            // Accept TCP connections
            if (controller->config.enable_tcp && controller->backend->tcp_listen_sock != -1 &&
                FD_ISSET(controller->backend->tcp_listen_sock, &fds)) {
                tcp_adapter_accept_client(controller->backend);
            }
            
            // Handle TCP clients
            if (controller->config.enable_tcp) {
                for (int i = 0; i < MAX_TCP_CLIENTS; i++) {
                    if (controller->backend->tcp_conn_socks[i] != -1 &&
                        FD_ISSET(controller->backend->tcp_conn_socks[i], &fds)) {
                        tcp_adapter_handle_client(controller->backend, i, query);
                    }
                }
            }
            
            // Handle RTU
            if (controller->config.enable_rtu && controller->backend->ctx_rtu) {
                int rtu_fd = modbus_get_socket(controller->backend->ctx_rtu);
                if (rtu_fd != -1 && FD_ISSET(rtu_fd, &fds)) {
                    rtu_adapter_handle(controller->backend, query);
                }
            }
            
            // RTU auto-reconnect
            if (controller->config.enable_rtu && !controller->backend->ctx_rtu) {
                rtu_adapter_reconnect(controller->backend, &controller->config);
            }
        }
#endif
    }
    
    // Final cleanup
    shutdown_server(controller);
    
    return 0;
}
