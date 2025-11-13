#ifndef BYTE_ORDER_H
#define BYTE_ORDER_H

#include <stdint.h>

typedef enum { ORDER_LE, ORDER_BE, ORDER_SWAP } ByteOrder;

/**
 * Parse byte order string
 * @param s Byte order string: "LE", "BE", or "SWAP"
 * @return ByteOrder enum value
 */
ByteOrder parse_byte_order(const char *s);

/**
 * Write multi-word data into registers with correct byte order
 * @param start_idx Starting register index
 * @param words Array of 16-bit words
 * @param count Number of words
 * @param order Byte order to apply
 * @param registers Target register array (assumed valid)
 */
void write_registers(int start_idx, uint16_t *words, int count, ByteOrder order, uint16_t *registers);

#endif // BYTE_ORDER_H
