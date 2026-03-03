#include <string.h>

#include <cbor.h>

#include "test_support.h"

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

int fake_read(void *ctx, uint8_t *buf, size_t max_len) {
    struct fake_io *io = (struct fake_io *)ctx;
    size_t available;
    size_t n;

    if (io->rx_off >= io->rx_len) {
        return 0;
    }

    available = io->rx_len - io->rx_off;
    n = available;
    if (n > max_len) {
        n = max_len;
    }
    if (io->max_read_chunk > 0 && n > io->max_read_chunk) {
        n = io->max_read_chunk;
    }

    memcpy(buf, io->rx + io->rx_off, n);
    io->rx_off += n;
    return (int)n;
}

int fake_write(void *ctx, const uint8_t *buf, size_t len) {
    struct fake_io *io = (struct fake_io *)ctx;
    size_t n = len;

    if (io->write_fail) {
        return -1;
    }

    if (io->max_write_chunk > 0 && n > io->max_write_chunk) {
        n = io->max_write_chunk;
    }

    memcpy(io->tx + io->tx_len, buf, n);
    io->tx_len += n;
    return (int)n;
}

int fake_slot_begin(void *ctx) {
    struct fake_platform *p = (struct fake_platform *)ctx;
    p->begin_calls++;
    return p->fail_begin ? -1 : 0;
}

int fake_slot_write(void *ctx, uint32_t off, const uint8_t *data, size_t len) {
    struct fake_platform *p = (struct fake_platform *)ctx;
    p->write_calls++;
    if (p->fail_write) {
        return -1;
    }
    memcpy(p->storage + off, data, len);
    return 0;
}

int fake_slot_finalize(void *ctx, size_t total_size) {
    struct fake_platform *p = (struct fake_platform *)ctx;
    p->finalize_calls++;
    p->finalized_total = total_size;
    return p->fail_finalize ? -1 : 0;
}

int fake_set_pending(void *ctx) {
    struct fake_platform *p = (struct fake_platform *)ctx;
    p->pending_calls++;
    return p->fail_pending ? -1 : 0;
}

size_t make_upload_req(uint8_t *out,
                       uint8_t seq,
                       uint32_t off,
                       const uint8_t *data,
                       uint16_t data_len,
                       uint32_t total_len) {
    CborEncoder encoder;
    CborEncoder map;
    size_t pairs = (total_len > 0u) ? 3u : 2u;
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
    cbor_encode_byte_string(&map, data, data_len);
    if (total_len > 0u) {
        cbor_encode_text_stringz(&map, "len");
        cbor_encode_uint(&map, total_len);
    }
    cbor_encoder_close_container_checked(&encoder, &map);

    payload_len = cbor_encoder_get_buffer_size(&encoder, out + SMP_HEADER_SIZE);
    put_be16(out + 2, (uint16_t)payload_len);
    return payload_len + SMP_HEADER_SIZE;
}

uint16_t resp_rc(const uint8_t *frame) {
    uint32_t rc = UINT32_MAX;
    if (parse_resp_u32(frame, "rc", &rc) != 0) {
        return UINT16_MAX;
    }
    return (uint16_t)rc;
}

uint32_t resp_expected_off(const uint8_t *frame) {
    uint32_t off = 0u;
    if (parse_resp_u32(frame, "off", &off) != 0) {
        return 0u;
    }
    return off;
}
