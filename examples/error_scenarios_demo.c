#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "smp/smp.h"
#include <cbor.h>

#define IMAGE_SIZE 64u

static void put_be16(uint8_t *p, uint16_t v);
static uint16_t be16(const uint8_t *p);

struct mock_io {
    uint8_t rx_buf[256];
    size_t rx_len;
    size_t rx_off;
    uint8_t tx_buf[256];
    size_t tx_len;
};

struct mock_platform {
    uint8_t storage[IMAGE_SIZE];
    size_t storage_len;
    uint32_t total_size;
    int begin_calls;
    int write_calls;
    int finalize_calls;
    int pending_calls;
    int fail_begin;
    int fail_write;
    int fail_finalize;
    int fail_pending;
};

static int transport_read(void *ctx, uint8_t *buf, size_t max_len) {
    struct mock_io *io = (struct mock_io *)ctx;
    if (io->rx_off >= io->rx_len) {
        return 0;
    }
    size_t available = io->rx_len - io->rx_off;
    size_t n = available;
    if (n > max_len) {
        n = max_len;
    }
    memcpy(buf, io->rx_buf + io->rx_off, n);
    io->rx_off += n;
    return (int)n;
}

static int transport_write(void *ctx, const uint8_t *buf, size_t len) {
    struct mock_io *io = (struct mock_io *)ctx;
    if (io->tx_len + len > sizeof(io->tx_buf)) {
        return -1;
    }
    memcpy(io->tx_buf + io->tx_len, buf, len);
    io->tx_len += len;
    return (int)len;
}

static int slot_begin(void *ctx) {
    struct mock_platform *plat = (struct mock_platform *)ctx;
    plat->begin_calls++;
    plat->storage_len = 0u;
    return plat->fail_begin ? -1 : 0;
}

static int slot_write(void *ctx, uint32_t off, const uint8_t *data, size_t len) {
    struct mock_platform *plat = (struct mock_platform *)ctx;
    plat->write_calls++;
    if (plat->fail_write) {
        return -1;
    }
    if (off + len > sizeof(plat->storage)) {
        return -1;
    }
    memcpy(plat->storage + off, data, len);
    return 0;
}

static int slot_finalize(void *ctx, size_t total_size) {
    struct mock_platform *plat = (struct mock_platform *)ctx;
    plat->finalize_calls++;
    plat->total_size = (uint32_t)total_size;
    return plat->fail_finalize ? -1 : 0;
}

static int set_pending(void *ctx) {
    struct mock_platform *plat = (struct mock_platform *)ctx;
    plat->pending_calls++;
    return plat->fail_pending ? -1 : 0;
}

static size_t make_upload_req(uint8_t *out, uint8_t seq, uint32_t off,
                               const uint8_t *data, uint16_t len, uint32_t total) {
    CborEncoder encoder;
    CborEncoder map;
    size_t pairs = (total > 0u) ? 3u : 2u;
    size_t payload_len;

    out[0] = 2u;
    out[1] = 0u;
    put_be16(out + 4, SMP_IMAGE_GROUP_ID);
    out[6] = seq;
    out[7] = SMP_IMAGE_UPLOAD_ID;

    cbor_encoder_init(&encoder, out + SMP_HEADER_SIZE, 1024u, 0);
    cbor_encoder_create_map(&encoder, &map, pairs);
    cbor_encode_text_stringz(&map, "off");
    cbor_encode_uint(&map, off);
    cbor_encode_text_stringz(&map, "data");
    cbor_encode_byte_string(&map, data, len);
    if (total > 0u) {
        cbor_encode_text_stringz(&map, "len");
        cbor_encode_uint(&map, total);
    }
    cbor_encoder_close_container_checked(&encoder, &map);

    payload_len = cbor_encoder_get_buffer_size(&encoder, out + SMP_HEADER_SIZE);
    put_be16(out + 2, (uint16_t)payload_len);
    return payload_len + SMP_HEADER_SIZE;
}

static size_t make_unknown_cmd_req(uint8_t *out, uint8_t seq, uint16_t group, uint8_t id) {
    out[0] = 2u;
    out[1] = 0u;
    out[2] = 0u;
    out[3] = 0u;
    put_be16(out + 4, group);
    out[6] = seq;
    out[7] = id;
    return SMP_HEADER_SIZE;
}

static void put_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)((v >> 8) & 0xffu);
    p[1] = (uint8_t)(v & 0xffu);
}

static uint16_t be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static int parse_resp_u32(const uint8_t *frame, const char *key, uint32_t *out) {
    CborParser parser;
    CborValue root;
    CborValue map;

    if (cbor_parser_init(frame + SMP_HEADER_SIZE, (size_t)be16(frame + 2), 0, &parser, &root) != CborNoError) {
        return -1;
    }
    if (!cbor_value_is_map(&root)) {
        return -1;
    }
    if (cbor_value_enter_container(&root, &map) != CborNoError) {
        return -1;
    }

    while (!cbor_value_at_end(&map)) {
        char k[16];
        size_t klen = sizeof(k) - 1u;
        CborValue next;
        uint64_t v;

        if (!cbor_value_is_text_string(&map)) {
            return -1;
        }
        if (cbor_value_copy_text_string(&map, k, &klen, &next) != CborNoError) {
            return -1;
        }
        k[klen] = '\0';
        map = next;

        if (strcmp(k, key) == 0) {
            if (!cbor_value_is_integer(&map) || cbor_value_is_negative_integer(&map)) {
                return -1;
            }
            if (cbor_value_get_uint64(&map, &v) != CborNoError) {
                return -1;
            }
            *out = (uint32_t)v;
            return 0;
        }

        if (cbor_value_advance(&map) != CborNoError) {
            return -1;
        }
    }

    return -1;
}

static int test_invalid_offset(void) {
    struct mock_io io = {0};
    struct mock_platform plat = {0};
    struct smp_transport_io t = { .read = transport_read, .write = transport_write, .ctx = &io };
    struct smp_platform_ops p = { .slot_begin = slot_begin, .slot_write = slot_write,
                                   .slot_finalize = slot_finalize, .set_pending = set_pending, .ctx = &plat };
    struct smp_ctx ctx;
    uint8_t req[128];
    uint8_t chunk1[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t chunk2[] = {0x05, 0x06, 0x07, 0x08};
    size_t req_len;
    uint32_t resp_off;
    enum smp_err rc;

    if (smp_init(&ctx, &t, &p, NULL) != 0) {
        return -1;
    }

    req_len = make_upload_req(req, 0, 0, chunk1, 4, 8);
    memcpy(io.rx_buf, req, req_len);
    io.rx_len = req_len;
    io.rx_off = 0u;
    io.tx_len = 0u;
    while (ctx.tx_len != 0u || io.rx_off < io.rx_len || ctx.rx_len != 0u) {
        if (smp_poll(&ctx) != 0) return -1;
    }
    if (parse_resp_u32(io.tx_buf, "rc", &rc) != 0 || parse_resp_u32(io.tx_buf, "off", &resp_off) != 0) {
        return -1;
    }
    if (rc != SMP_ERR_OK || resp_off != 4) {
        return -1;
    }

    req_len = make_upload_req(req, 1, 99, chunk2, 4, 8);
    memcpy(io.rx_buf, req, req_len);
    io.rx_len = req_len;
    io.rx_off = 0u;
    io.tx_len = 0u;
    while (ctx.tx_len != 0u || io.rx_off < io.rx_len || ctx.rx_len != 0u) {
        if (smp_poll(&ctx) != 0) return -1;
    }
    if (parse_resp_u32(io.tx_buf, "rc", &rc) != 0 || parse_resp_u32(io.tx_buf, "off", &resp_off) != 0) {
        return -1;
    }
    if (rc != SMP_ERR_INVALID_OFFSET || resp_off != 4) {
        return -1;
    }

    req_len = make_upload_req(req, 2, 4, chunk2, 4, 8);
    memcpy(io.rx_buf, req, req_len);
    io.rx_len = req_len;
    io.rx_off = 0u;
    io.tx_len = 0u;
    while (ctx.tx_len != 0u || io.rx_off < io.rx_len || ctx.rx_len != 0u) {
        if (smp_poll(&ctx) != 0) return -1;
    }
    if (parse_resp_u32(io.tx_buf, "rc", &rc) != 0 || parse_resp_u32(io.tx_buf, "off", &resp_off) != 0) {
        return -1;
    }
    if (rc != SMP_ERR_OK || resp_off != 8) {
        return -1;
    }

    return 0;
}

static int test_write_failure(void) {
    struct mock_io io = {0};
    struct mock_platform plat = {0};
    struct smp_transport_io t = { .read = transport_read, .write = transport_write, .ctx = &io };
    struct smp_platform_ops p = { .slot_begin = slot_begin, .slot_write = slot_write,
                                   .slot_finalize = slot_finalize, .set_pending = set_pending, .ctx = &plat };
    struct smp_ctx ctx;
    uint8_t req[128];
    uint8_t chunk[] = {0x10, 0x20, 0x30, 0x40};
    size_t req_len;
    uint32_t resp_off;
    enum smp_err rc;

    plat.fail_write = 1;
    if (smp_init(&ctx, &t, &p, NULL) != 0) {
        return -1;
    }

    req_len = make_upload_req(req, 0, 0, chunk, 4, 4);
    memcpy(io.rx_buf, req, req_len);
    io.rx_len = req_len;
    io.rx_off = 0u;
    io.tx_len = 0u;
    while (ctx.tx_len != 0u || io.rx_off < io.rx_len || ctx.rx_len != 0u) {
        if (smp_poll(&ctx) != 0) return -1;
    }
    if (parse_resp_u32(io.tx_buf, "rc", &rc) != 0 || parse_resp_u32(io.tx_buf, "off", &resp_off) != 0) {
        return -1;
    }
    if (rc != SMP_ERR_IO) {
        return -1;
    }

    return 0;
}

static int test_finalize_failure(void) {
    struct mock_io io = {0};
    struct mock_platform plat = {0};
    struct smp_transport_io t = { .read = transport_read, .write = transport_write, .ctx = &io };
    struct smp_platform_ops p = { .slot_begin = slot_begin, .slot_write = slot_write,
                                   .slot_finalize = slot_finalize, .set_pending = set_pending, .ctx = &plat };
    struct smp_ctx ctx;
    uint8_t req[128];
    uint8_t chunk[] = {0x10, 0x20};
    size_t req_len;
    uint32_t resp_off;
    enum smp_err rc;

    plat.fail_finalize = 1;
    if (smp_init(&ctx, &t, &p, NULL) != 0) {
        return -1;
    }

    req_len = make_upload_req(req, 0, 0, chunk, 2, 2);
    memcpy(io.rx_buf, req, req_len);
    io.rx_len = req_len;
    io.rx_off = 0u;
    io.tx_len = 0u;
    while (ctx.tx_len != 0u || io.rx_off < io.rx_len || ctx.rx_len != 0u) {
        if (smp_poll(&ctx) != 0) return -1;
    }
    if (parse_resp_u32(io.tx_buf, "rc", &rc) != 0 || parse_resp_u32(io.tx_buf, "off", &resp_off) != 0) {
        return -1;
    }
    if (rc != SMP_ERR_IO) {
        return -1;
    }
    if (plat.finalize_calls != 1 || plat.pending_calls != 0) {
        return -1;
    }

    return 0;
}

static int test_pending_failure(void) {
    struct mock_io io = {0};
    struct mock_platform plat = {0};
    struct smp_transport_io t = { .read = transport_read, .write = transport_write, .ctx = &io };
    struct smp_platform_ops p = { .slot_begin = slot_begin, .slot_write = slot_write,
                                   .slot_finalize = slot_finalize, .set_pending = set_pending, .ctx = &plat };
    struct smp_ctx ctx;
    uint8_t req[128];
    uint8_t chunk[] = {0x10, 0x20};
    size_t req_len;
    uint32_t resp_off;
    enum smp_err rc;

    plat.fail_pending = 1;
    if (smp_init(&ctx, &t, &p, NULL) != 0) {
        return -1;
    }

    req_len = make_upload_req(req, 0, 0, chunk, 2, 2);
    memcpy(io.rx_buf, req, req_len);
    io.rx_len = req_len;
    io.rx_off = 0u;
    io.tx_len = 0u;
    while (ctx.tx_len != 0u || io.rx_off < io.rx_len || ctx.rx_len != 0u) {
        if (smp_poll(&ctx) != 0) return -1;
    }
    if (parse_resp_u32(io.tx_buf, "rc", &rc) != 0 || parse_resp_u32(io.tx_buf, "off", &resp_off) != 0) {
        return -1;
    }
    if (rc != SMP_ERR_IO) {
        return -1;
    }
    if (plat.finalize_calls != 1 || plat.pending_calls != 1) {
        return -1;
    }

    return 0;
}

static int test_unknown_command(void) {
    struct mock_io io = {0};
    struct mock_platform plat = {0};
    struct smp_transport_io t = { .read = transport_read, .write = transport_write, .ctx = &io };
    struct smp_platform_ops p = { .slot_begin = slot_begin, .slot_write = slot_write,
                                   .slot_finalize = slot_finalize, .set_pending = set_pending, .ctx = &plat };
    struct smp_ctx ctx;
    uint8_t req[16];
    size_t req_len;
    uint32_t resp_rc;

    if (smp_init(&ctx, &t, &p, NULL) != 0) {
        return -1;
    }

    req_len = make_unknown_cmd_req(req, 0, 99, 77);
    memcpy(io.rx_buf, req, req_len);
    io.rx_len = req_len;
    io.rx_off = 0u;
    io.tx_len = 0u;
    while (ctx.tx_len != 0u || io.rx_off < io.rx_len || ctx.rx_len != 0u) {
        if (smp_poll(&ctx) != 0) return -1;
    }
    if (parse_resp_u32(io.tx_buf, "rc", &resp_rc) != 0) {
        return -1;
    }
    if (resp_rc != SMP_ERR_NOT_SUPPORTED) {
        return -1;
    }

    return 0;
}

int main(void) {
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        { "invalid_offset", test_invalid_offset },
        { "write_failure", test_write_failure },
        { "finalize_failure", test_finalize_failure },
        { "pending_failure", test_pending_failure },
        { "unknown_command", test_unknown_command },
    };
    size_t i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        printf("Testing: %s... ", tests[i].name);
        if (tests[i].fn() == 0) {
            printf("PASS\n");
        } else {
            printf("FAIL\n");
            return 1;
        }
    }

    printf("\nAll tests passed\n");
    return 0;
}
