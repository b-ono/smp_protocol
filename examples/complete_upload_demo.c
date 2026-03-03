#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "smp/smp.h"
#include <cbor.h>

#define IMAGE_SIZE 256u
#define CHUNK_SIZE 64u

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
    return 0;
}

static int slot_write(void *ctx, uint32_t off, const uint8_t *data, size_t len) {
    struct mock_platform *plat = (struct mock_platform *)ctx;
    plat->write_calls++;
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
    return 0;
}

static int set_pending(void *ctx) {
    struct mock_platform *plat = (struct mock_platform *)ctx;
    plat->pending_calls++;
    return 0;
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

static void print_upload_progress(uint32_t off, uint32_t total, enum smp_err rc, uint32_t expected_off) {
    printf("  Chunk: offset=%u, total_size=%u, rc=%u, expected_off=%u\n",
           off, total, rc, expected_off);
}

int main(void) {
    struct mock_io io = {0};
    struct mock_platform plat = {0};
    struct smp_transport_io t = {
        .read = transport_read,
        .write = transport_write,
        .ctx = &io,
    };
    struct smp_platform_ops p = {
        .slot_begin = slot_begin,
        .slot_write = slot_write,
        .slot_finalize = slot_finalize,
        .set_pending = set_pending,
        .ctx = &plat,
    };
    struct smp_ctx ctx;
    uint8_t req[256];
    uint8_t image[IMAGE_SIZE];
    uint8_t seq = 0u;
    uint32_t off = 0u;
    size_t req_len;
    uint32_t resp_off;
    enum smp_err rc;
    int i;

    for (i = 0; i < (int)IMAGE_SIZE; ++i) {
        image[i] = (uint8_t)(i & 0xffu);
    }

    if (smp_init(&ctx, &t, &p, NULL) != 0) {
        printf("smp_init failed\n");
        return 1;
    }

    printf("Starting 256-byte image upload (4 chunks of 64 bytes)\n");
    printf("Image pattern: 0x00, 0x01, 0x02, ... 0xFF, 0x00, ...\n\n");

    while (off < IMAGE_SIZE) {
        uint32_t chunk_len = IMAGE_SIZE - off;
        if (chunk_len > CHUNK_SIZE) {
            chunk_len = CHUNK_SIZE;
        }

        req_len = make_upload_req(req, seq, off, image + off, (uint16_t)chunk_len, IMAGE_SIZE);

        memcpy(io.rx_buf, req, req_len);
        io.rx_len = req_len;
        io.rx_off = 0u;
        io.tx_len = 0u;

        while (ctx.tx_len != 0u || io.rx_off < io.rx_len || ctx.rx_len != 0u) {
            if (smp_poll(&ctx) != 0) {
                printf("smp_poll error\n");
                return 1;
            }
        }

        if (parse_resp_u32(io.tx_buf, "rc", &rc) != 0) {
            printf("failed to parse rc\n");
            return 1;
        }
        if (parse_resp_u32(io.tx_buf, "off", &resp_off) != 0) {
            printf("failed to parse off\n");
            return 1;
        }

        print_upload_progress(off, IMAGE_SIZE, rc, resp_off);

        off = resp_off;
        seq++;
    }

    printf("\nUpload complete\n");
    printf("Platform calls: begin=%d, write=%d, finalize=%d, pending=%d\n",
           plat.begin_calls, plat.write_calls, plat.finalize_calls, plat.pending_calls);
    printf("Finalized size: %u bytes\n", plat.total_size);

    if (plat.finalize_calls != 1 || plat.pending_calls != 1) {
        printf("ERROR: finalize/pending not called correctly\n");
        return 1;
    }

    if (plat.total_size != IMAGE_SIZE) {
        printf("ERROR: total_size mismatch\n");
        return 1;
    }

    if (memcmp(plat.storage, image, IMAGE_SIZE) != 0) {
        printf("ERROR: storage mismatch\n");
        return 1;
    }

    printf("Storage verification: PASS\n");
    printf("\nAll tests passed\n");

    return 0;
}
