#include "smp/smp_base64.h"

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
    size_t valid_chars = 0;
    int padding = 0;
    
    /* Skip trailing newlines */
    while (in_len > 0 && (in[in_len - 1] == '\n' || in[in_len - 1] == '\r')) {
        in_len--;
    }
    
    if (in_len == 0) {
        *out_len = 0;
        return 0;
    }
    
    /* Count padding and validate length */
    if (in[in_len - 1] == '=') {
        padding++;
        if (in[in_len - 2] == '=') {
            padding++;
        }
    }
    
    /* Length must be multiple of 4 */
    if (in_len % 4 != 0) {
        return -1;
    }
    
for (i = 0, j = 0; i < in_len; i += 4, j += 3) {
        int8_t sextet_a = (i < in_len) ? 
                          smp_base64_decode_table[(uint8_t)in[i]] : -1;
        int8_t sextet_b = (i + 1 < in_len) ? 
                          smp_base64_decode_table[(uint8_t)in[i + 1]] : -1;
        int8_t sextet_c = (i + 2 < in_len && in[i + 2] != '=') ? 
                          smp_base64_decode_table[(uint8_t)in[i + 2]] : 0;
        int8_t sextet_d = (i + 3 < in_len && in[i + 3] != '=') ? 
                          smp_base64_decode_table[(uint8_t)in[i + 3]] : 0;
        
        /* Check for invalid characters */
        if ((sextet_a == -1 && i < in_len) || 
            (sextet_b == -1 && i + 1 < in_len && in[i + 1] != '=')) {
            return -1;
        }
        
        /* Check padding alignment */
        if (sextet_a == -1 || sextet_b == -1) {
            return -1;
        }
        
        uint32_t triple = (sextet_a << 18) | (sextet_b << 12) | 
                          (sextet_c << 6) | sextet_d;
        
        out[j + 0] = (triple >> 16) & 0xFF;
        if (i + 2 < in_len && in[i + 2] != '=') {
            out[j + 1] = (triple >> 8) & 0xFF;
        }
        if (i + 3 < in_len && in[i + 3] != '=') {
            out[j + 2] = triple & 0xFF;
        }
        
        valid_chars += 4;
    }
    
    /* Calculate actual output length based on padding */
    if (padding == 2) {
        *out_len = ((in_len / 4) - 1) * 3 + 1;
    } else if (padding == 1) {
        *out_len = (in_len / 4) * 3 - 1;
    } else {
        *out_len = (in_len / 4) * 3;
    }
    return 0;
}
