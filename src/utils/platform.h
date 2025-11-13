#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdio.h>

#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <io.h>
    
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_LINUX 0
    
    typedef HANDLE platform_handle_t;
    typedef unsigned long platform_socket_t;
    
    #define STDIN_FILENO 0
    #define STDOUT_FILENO 1
    #define STDERR_FILENO 2
    
    #define platform_msleep(ms) Sleep(ms)
    #define platform_close_fd(fd) _close(fd)
    
#else
    #include <unistd.h>
    #include <sys/select.h>
    #include <sys/types.h>
    #include <sys/time.h>
    #include <fcntl.h>
    
    #define PLATFORM_WINDOWS 0
    #define PLATFORM_LINUX 1
    
    typedef int platform_handle_t;
    typedef int platform_socket_t;
    
    #define platform_msleep(ms) { struct timespec ts; ts.tv_sec = (ms)/1000; ts.tv_nsec = ((ms)%1000)*1000000; nanosleep(&ts, NULL); }
    #define platform_close_fd(fd) close(fd)
    
#endif

/**
 * Initialize platform-specific features (Windows: Winsock, Linux: nothing)
 * @return 0 on success, -1 on failure
 */
int platform_init(void);

/**
 * Cleanup platform-specific features
 */
void platform_cleanup(void);

/**
 * Platform-aware stdin read with timeout
 * @param buf Buffer to read into
 * @param size Size of buffer
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes read, 0 if timeout, -1 if error
 */
int platform_read_stdin(char *buf, size_t size, int timeout_ms);

/**
 * Set file descriptor to non-blocking mode
 * @param fd File descriptor
 * @return 0 on success, -1 on failure
 */
int platform_set_nonblocking(int fd);

/**
 * Get number of available processors
 * @return Number of processors
 */
int platform_get_nprocs(void);

#endif // PLATFORM_H
