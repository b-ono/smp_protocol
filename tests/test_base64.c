#include <stdio.h>
#include <smp/smp_base64.h>
#include <string.h>

typedef int (*test_fn)(void);

static int test_base64_encode_empty(void)
{
    char out[1] = {0};
    size_t out_len = 99;

    if (smp_base64_encode_size(0) != 0) {
        return 1;
    }
    if (smp_base64_encode(NULL, 0, out, sizeof(out), &out_len) != 0) {
        return 1;
    }
    if (out_len != 0) {
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

    if (smp_base64_decode_size(0) != 0) {
        return 1;
    }
    if (smp_base64_decode("", 0, out, sizeof(out), &out_len) != 0) {
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

static int test_base64_decode_rejects_invalid_third_char(void)
{
    uint8_t out[4];
    size_t out_len;

    if (smp_base64_decode("QU?D", 4, out, sizeof(out), &out_len) != -1) {
        return 1;
    }
    return 0;
}

static int test_base64_decode_rejects_invalid_fourth_char(void)
{
    uint8_t out[4];
    size_t out_len;

    if (smp_base64_decode("QUJ?", 4, out, sizeof(out), &out_len) != -1) {
        return 1;
    }
    return 0;
}

static int test_base64_decode_rejects_non_ascii_input(void)
{
    uint8_t out[4];
    size_t out_len;
    char in[4] = {'Q', 'U', (char)0xFF, 'D'};

    if (smp_base64_decode(in, sizeof(in), out, sizeof(out), &out_len) != -1) {
        return 1;
    }
    return 0;
}

static int test_base64_decode_respects_output_capacity(void)
{
    uint8_t out[1];
    size_t out_len;

    if (smp_base64_decode("QUI=", 4, out, sizeof(out), &out_len) != -1) {
        return 1;
    }
    return 0;
}

int run_base64_tests(void)
{
    const struct {
        const char *name;
        test_fn fn;
    } tests[] = {
        { "base64_encode_empty", test_base64_encode_empty },
        { "base64_encode_single_byte", test_base64_encode_single_byte },
        { "base64_encode_two_bytes", test_base64_encode_two_bytes },
        { "base64_encode_three_bytes", test_base64_encode_three_bytes },
        { "base64_encode_four_bytes", test_base64_encode_four_bytes },
        { "base64_decode_empty", test_base64_decode_empty },
        { "base64_decode_single", test_base64_decode_single },
        { "base64_decode_two", test_base64_decode_two },
        { "base64_decode_three", test_base64_decode_three },
        { "base64_roundtrip", test_base64_roundtrip },
        { "base64_buffer_overflow_encode", test_base64_buffer_overflow_encode },
        { "base64_buffer_overflow_decode", test_base64_buffer_overflow_decode },
        { "base64_invalid_char_decode", test_base64_invalid_char_decode },
        { "base64_misaligned_padding_decode", test_base64_misaligned_padding_decode },
        { "base64_newline_handling", test_base64_newline_handling },
        { "base64_invalid_third_char", test_base64_decode_rejects_invalid_third_char },
        { "base64_invalid_fourth_char", test_base64_decode_rejects_invalid_fourth_char },
        { "base64_non_ascii_input", test_base64_decode_rejects_non_ascii_input },
        { "base64_decode_capacity", test_base64_decode_respects_output_capacity },
    };
    size_t i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (tests[i].fn() != 0) {
            printf("FAIL %s\n", tests[i].name);
            return 1;
        }
    }
    return 0;
}
