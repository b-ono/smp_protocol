#include "smp/smp_base64.h"

#include <stdbool.h>

/* RFC 4648 base64 encoding table */
const char smp_base64_encode_table[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* RFC 4648 base64 decoding table (-1 for invalid chars) */
const int8_t smp_base64_decode_table[] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
};

size_t smp_base64_encode_size(size_t in_len)
{
    return ((in_len + 2) / 3) * 4;
}

int smp_base64_encode(const uint8_t *in, size_t in_len, 
                      char *out, size_t out_capacity, size_t *out_len)
{
    size_t i, j;
    size_t required = smp_base64_encode_size(in_len);
    
    if (out_capacity < required) {
        return -1;
    }
    
    for (i = 0, j = 0; i < in_len; i += 3, j += 4) {
        uint32_t octet_a = i < in_len ? in[i] : 0;
        uint32_t octet_b = i + 1 < in_len ? in[i + 1] : 0;
        uint32_t octet_c = i + 2 < in_len ? in[i + 2] : 0;
        
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        out[j + 0] = smp_base64_encode_table[(triple >> 18) & 0x3F];
        out[j + 1] = smp_base64_encode_table[(triple >> 12) & 0x3F];
        out[j + 2] = (i + 1 < in_len) ? 
                     smp_base64_encode_table[(triple >> 6) & 0x3F] : '=';
        out[j + 3] = (i + 2 < in_len) ? 
                     smp_base64_encode_table[triple & 0x3F] : '=';
    }
    
    *out_len = j;
    return 0;
}

size_t smp_base64_decode_size(size_t in_len)
{
    return (in_len / 4) * 3;
}

int smp_base64_decode(const char *in, size_t in_len,
                      uint8_t *out, size_t out_capacity, size_t *out_len)
{
    size_t i, j;
    size_t padding = 0;
    size_t required;
    
    /* Skip trailing newlines */
    while (in_len > 0 && (in[in_len - 1] == '\n' || in[in_len - 1] == '\r')) {
        in_len--;
    }
    
    if (in_len == 0) {
        *out_len = 0;
        return 0;
    }
    
    /* Length must be multiple of 4 */
    if (in_len % 4 != 0) {
        return -1;
    }

    for (i = 0; i < in_len; ++i) {
        uint8_t ch = (uint8_t)in[i];
        bool is_last_block = i >= (in_len - 4);
        bool can_be_padding = (i % 4) >= 2;

        if (ch == '=') {
            if (!is_last_block || !can_be_padding) {
                return -1;
            }
            ++padding;
            continue;
        }

        if (padding != 0 || ch > 127 || smp_base64_decode_table[ch] < 0) {
            return -1;
        }
    }

    if (padding > 2) {
        return -1;
    }
    if (padding == 1 && in[in_len - 1] != '=') {
        return -1;
    }
    if (padding == 2 && (in[in_len - 2] != '=' || in[in_len - 1] != '=')) {
        return -1;
    }

    required = (in_len / 4) * 3 - padding;
    if (out_capacity < required) {
        return -1;
    }

    for (i = 0, j = 0; i < in_len; i += 4) {
        int8_t sextet_a = smp_base64_decode_table[(uint8_t)in[i + 0]];
        int8_t sextet_b = smp_base64_decode_table[(uint8_t)in[i + 1]];
        int8_t sextet_c = (in[i + 2] == '=') ? 0 : smp_base64_decode_table[(uint8_t)in[i + 2]];
        int8_t sextet_d = (in[i + 3] == '=') ? 0 : smp_base64_decode_table[(uint8_t)in[i + 3]];

        if (sextet_a < 0 || sextet_b < 0 || sextet_c < 0 || sextet_d < 0) {
            return -1;
        }

        uint32_t triple = (sextet_a << 18) | (sextet_b << 12) |
                          (sextet_c << 6) | sextet_d;

        out[j++] = (triple >> 16) & 0xFF;
        if (i + 2 < in_len && in[i + 2] != '=') {
            out[j++] = (triple >> 8) & 0xFF;
        }
        if (i + 3 < in_len && in[i + 3] != '=') {
            out[j++] = triple & 0xFF;
        }
    }

    *out_len = j;
    return 0;
}
