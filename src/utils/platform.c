#include "platform.h"
#include "logging.h"

#ifdef _WIN32
#include <stdio.h>
#include <conio.h>

static WSADATA wsaData;

int platform_init(void) {
    log_debug("Initializing Windows platform");
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        log_error("WSAStartup failed: %d", result);
        return -1;
    }
    log_debug("Winsock initialized successfully");
    return 0;
}

void platform_cleanup(void) {
    log_debug("Cleaning up Windows platform");
    WSACleanup();
}

int platform_read_stdin(char *buf, size_t size, int timeout_ms) {
    // Windows: Simple non-blocking read using _kbhit()
    // For production, could use WaitForMultipleObjects with pipes
    
    if (_kbhit()) {
        if (fgets(buf, size, stdin)) {
            return (int)strlen(buf);
        }
        return -1;
    }
    return 0;
}

int platform_set_nonblocking(int fd) {
    unsigned long mode = 1;
    // For Winsock sockets
    if (ioctlsocket(fd, FIONBIO, &mode) == SOCKET_ERROR) {
        log_error("Failed to set non-blocking mode on socket: %d", WSAGetLastError());
        return -1;
    }
    return 0;
}

int platform_get_nprocs(void) {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
}

#else // Linux/Unix

#include <errno.h>
#include <time.h>

int platform_init(void) {
    log_debug("Initializing Linux/Unix platform");
    // No special initialization needed for Linux
    return 0;
}

void platform_cleanup(void) {
    log_debug("Cleaning up Linux/Unix platform");
    // No special cleanup needed for Linux
}

int platform_read_stdin(char *buf, size_t size, int timeout_ms) {
    fd_set readfds;
    struct timeval tv;
    
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
    
    if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        if (fgets(buf, size, stdin)) {
            return (int)strlen(buf);
        }
    }
    
    return ret < 0 ? -1 : 0;
}

int platform_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        log_error("fcntl F_GETFL failed: %s", strerror(errno));
        return -1;
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        log_error("fcntl F_SETFL failed: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

int platform_get_nprocs(void) {
    // Try sysconf first
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs > 0) {
        return (int)nprocs;
    }
    
    // Fallback to 1
    return 1;
}

#endif
