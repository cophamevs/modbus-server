#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

#define log_debug(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define log_info(fmt, ...)  fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)
#define log_error(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define log_warn(fmt, ...)  fprintf(stderr, "[WARN] " fmt "\n", ##__VA_ARGS__)

#endif // LOGGING_H
