#include "byte_order.h"
#include <string.h>
#include <ctype.h>

ByteOrder parse_byte_order(const char *s) {
    if (!s || strcasecmp(s, "LE") == 0) return ORDER_LE;
    if (strcasecmp(s, "BE") == 0) return ORDER_BE;
    if (strcasecmp(s, "SWAP") == 0) return ORDER_SWAP;
    return ORDER_LE;
}

void write_registers(int start_idx, uint16_t *words, int count, ByteOrder order, uint16_t *registers) {
    // Swap bytes within words if SWAP
    if(order == ORDER_SWAP) {
        for(int i = 0; i < count; i++) {
            words[i] = (words[i] >> 8) | (words[i] << 8);
        }
    }
    // Swap word order if BE (for 2-word data)
    if(order == ORDER_BE && count == 2) {
        uint16_t tmp = words[0];
        words[0] = words[1];
        words[1] = tmp;
    }
    for(int i = 0; i < count; i++) {
        registers[start_idx + i] = words[i];
    }
}
