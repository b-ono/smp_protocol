#include <smp/smp_base64.h>
#include <string.h>

typedef int (*test_fn)(void);

static int test_base64_encode_empty(void)
{
    char out[4];
    size_t out_len;
    
    if (smp_base64_encode_size(0) != 4) {
        return 1;
    }
    if (smp_base64_encode(NULL, 0, out, 4, &out_len) != 0) {
        return 1;
    }
    if (out_len != 4) {
        return 1;
    }
    if (memcmp(out, "AAAA", 4) != 0) {
        return 1;
    }
    return 0;
}

static int test_base64_encode_single_byte(void)
{
    char out[4];
    size_t out_len;
    uint8_t in[] = {0x41};
    
    if (smp_base64_encode_size(1) != 4) {
        return 1;
    }
    if (smp_base64_encode(in, 1, out, 4, &out_len) != 0) {
        return 1;
    }
    if (out_len != 4) {
        return 1;
    }
    if (memcmp(out, "QQ==", 4) != 0) {
        return 1;
    }
    return 0;
}

static int test_base64_encode_two_bytes(void)
{
    char out[4];
    size_t out_len;
    uint8_t in[] = {0x41, 0x42};
    
    if (smp_base64_encode_size(2) != 4) {
        return 1;
    }
    if (smp_base64_encode(in, 2, out, 4, &out_len) != 0) {
        return 1;
    }
    if (out_len != 4) {
        return 1;
    }
    if (memcmp(out, "QUI=", 4) != 0) {
        return 1;
    }
    return 0;
}

static int test_base64_encode_three_bytes(void)
{
    char out[4];
    size_t out_len;
    uint8_t in[] = {0x41, 0x42, 0x43};
    
    if (smp_base64_encode_size(3) != 4) {
        return 1;
    }
    if (smp_base64_encode(in, 3, out, 4, &out_len) != 0) {
        return 1;
    }
    if (out_len != 4) {
        return 1;
    }
    if (memcmp(out, "QUJD", 4) != 0) {
        return 1;
    }
    return 0;
}

static int test_base64_encode_four_bytes(void)
{
    char out[8];
    size_t out_len;
    uint8_t in[] = {0x41, 0x42, 0x43, 0x44};
    
    if (smp_base64_encode_size(4) != 8) {
        return 1;
    }
    if (smp_base64_encode(in, 4, out, 8, &out_len) != 0) {
        return 1;
    }
    if (out_len != 8) {
        return 1;
    }
    if (memcmp(out, "QUJDRA==", 8) != 0) {
        return 1;
    }
    return 0;
}

static int test_base64_decode_empty(void)
{
    uint8_t out[4];
    size_t out_len;
    
    if (smp_base64_decode_size(4) != 3) {
        return 1;
    }
    if (smp_base64_decode("AAAA", 4, out, 4, &out_len) != 0) {
        return 1;
    }
    if (out_len != 0) {
        return 1;
    }
    return 0;
}

static int test_base64_decode_single(void)
{
    uint8_t out[4];
    size_t out_len;
    
    if (smp_base64_decode("QQ==", 4, out, 4, &out_len) != 0) {
        return 1;
    }
    if (out_len != 1) {
        return 1;
    }
    if (out[0] != 0x41) {
        return 1;
    }
    return 0;
}

static int test_base64_decode_two(void)
{
    uint8_t out[4];
    size_t out_len;
    
    if (smp_base64_decode("QUI=", 4, out, 4, &out_len) != 0) {
        return 1;
    }
    if (out_len != 2) {
        return 1;
    }
    if (memcmp(out, "AB", 2) != 0) {
        return 1;
    }
    return 0;
}

static int test_base64_decode_three(void)
{
    uint8_t out[4];
    size_t out_len;
    
    if (smp_base64_decode("QUJD", 4, out, 4, &out_len) != 0) {
        return 1;
    }
    if (out_len != 3) {
        return 1;
    }
    if (memcmp(out, "ABC", 3) != 0) {
        return 1;
    }
    return 0;
}

static int test_base64_roundtrip(void)
{
    uint8_t original[] = "Hello, World! This is a test.";
    char encoded[64];
    uint8_t decoded[64];
    size_t encoded_len, decoded_len;
    
    if (smp_base64_encode(original, sizeof(original) - 1, 
                         encoded, sizeof(encoded), &encoded_len) != 0) {
        return 1;
    }
    if (smp_base64_decode(encoded, encoded_len, 
                         decoded, sizeof(decoded), &decoded_len) != 0) {
        return 1;
    }
    if (decoded_len != sizeof(original) - 1) {
        return 1;
    }
    if (memcmp(original, decoded, decoded_len) != 0) {
        return 1;
    }
    return 0;
}

static int test_base64_buffer_overflow_encode(void)
{
    char out[3];
    size_t out_len;
    uint8_t in[] = {0x41};
    
    if (smp_base64_encode(in, 1, out, 3, &out_len) != -1) {
        return 1;
    }
    return 0;
}

static int test_base64_buffer_overflow_decode(void)
{
    uint8_t out[2];
    size_t out_len;
    
    if (smp_base64_decode("QUJD", 4, out, 2, &out_len) != -1) {
        return 1;
    }
    return 0;
}

static int test_base64_invalid_char_decode(void)
{
    uint8_t out[4];
    size_t out_len;
    
    if (smp_base64_decode("QQ=X", 4, out, 4, &out_len) != -1) {
        return 1;
    }
    return 0;
}

static int test_base64_misaligned_padding_decode(void)
{
    uint8_t out[4];
    size_t out_len;
    
    if (smp_base64_decode("QQ=", 3, out, 4, &out_len) != -1) {
        return 1;
    }
    return 0;
}

static int test_base64_newline_handling(void)
{
    uint8_t out[4];
    size_t out_len;
    
    if (smp_base64_decode("QQ==\n", 5, out, 4, &out_len) != 0) {
        return 1;
    }
    if (out_len != 1) {
        return 1;
    }
    return 0;
}
