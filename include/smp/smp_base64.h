#ifndef SMP_BASE64_H
#define SMP_BASE64_H

#include <stdint.h>
#include <stddef.h>

/**
 * Base64 encoding table (RFC 4648 standard)
 */
extern const char smp_base64_encode_table[];

/**
 * Base64 decoding table (reverse lookup)
 */
extern const int8_t smp_base64_decode_table[];

/**
 * Calculate required output buffer size for base64 encoding
 * 
 * @param in_len Length of input data in bytes
 * @return Required output buffer size (always multiple of 4)
 */
size_t smp_base64_encode_size(size_t in_len);

/**
 * Encode binary data to base64
 * 
 * @param in Input binary data
 * @param in_len Length of input data
 * @param out Output buffer (must be at least smp_base64_encode_size(in_len))
 * @param out_capacity Capacity of output buffer
 * @param out_len Pointer to store actual output length
 * @return 0 on success, -1 on error (buffer too small)
 */
int smp_base64_encode(const uint8_t *in, size_t in_len, 
                      char *out, size_t out_capacity, size_t *out_len);

/**
 * Calculate maximum output buffer size for base64 decoding
 * 
 * @param in_len Length of base64 input (including padding)
 * @return Maximum possible output buffer size
 */
size_t smp_base64_decode_size(size_t in_len);

/**
 * Decode base64 data to binary
 * 
 * @param in Base64 encoded string (may include newline terminators)
 * @param in_len Length of input data
 * @param out Output buffer (must be at least smp_base64_decode_size(in_len))
 * @param out_capacity Capacity of output buffer
 * @param out_len Pointer to store actual output length
 * @return 0 on success, -1 on error (invalid character or buffer too small)
 */
int smp_base64_decode(const char *in, size_t in_len,
                      uint8_t *out, size_t out_capacity, size_t *out_len);

#endif /* SMP_BASE64_H */
