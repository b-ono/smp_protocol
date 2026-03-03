#include <stdio.h>
#include <string.h>

#include "test_support.h"

typedef int (*test_fn)(void);

static int run_upload(struct smp_ctx *ctx, struct fake_io *io, const uint8_t *frame, size_t frame_len) {
    memcpy(io->rx, frame, frame_len);
    io->rx_len = frame_len;
    io->rx_off = 0u;
    io->tx_len = 0u;
    while (ctx->tx_len != 0u || io->rx_off < io->rx_len || ctx->rx_len != 0u) {
        if (smp_poll(ctx) != 0) {
            return -1;
        }
    }
    return 0;
}

static int test_api_contract(void) {
    struct fake_io io = {0};
    struct fake_platform plat = {0};
    struct smp_transport_io t = { .read = fake_read, .write = fake_write, .ctx = &io };
    struct smp_platform_ops p = { fake_slot_begin, fake_slot_write, fake_slot_finalize, fake_set_pending, &plat };
    struct smp_ctx ctx;
    return smp_init(&ctx, &t, &p, NULL);
}

static int test_upload_happy_path(void) {
    struct fake_io io = {0};
    struct fake_platform plat = {0};
    struct smp_transport_io t = { .read = fake_read, .write = fake_write, .ctx = &io };
    struct smp_platform_ops p = { fake_slot_begin, fake_slot_write, fake_slot_finalize, fake_set_pending, &plat };
    struct smp_ctx ctx;
    uint8_t req[64];
    const uint8_t chunk0[] = {0x11, 0x22, 0x33};
    const uint8_t chunk1[] = {0x44, 0x55};
    size_t n;

    TEST_ASSERT(smp_init(&ctx, &t, &p, NULL) == 0);

    n = make_upload_req(req, 1u, 0u, chunk0, (uint16_t)sizeof(chunk0),
                        (uint32_t)(sizeof(chunk0) + sizeof(chunk1)));
    TEST_ASSERT(run_upload(&ctx, &io, req, n) == 0);
    TEST_ASSERT(resp_rc(io.tx) == SMP_ERR_OK);
    TEST_ASSERT(resp_expected_off(io.tx) == sizeof(chunk0));

    n = make_upload_req(req, 2u, (uint32_t)sizeof(chunk0), chunk1, (uint16_t)sizeof(chunk1), 0u);
    TEST_ASSERT(run_upload(&ctx, &io, req, n) == 0);
    TEST_ASSERT(resp_rc(io.tx) == SMP_ERR_OK);
    TEST_ASSERT(resp_expected_off(io.tx) == sizeof(chunk0) + sizeof(chunk1));
    TEST_ASSERT(plat.finalize_calls == 1u);
    TEST_ASSERT(plat.pending_calls == 1u);
    TEST_ASSERT(ctx.upload_complete == 1u);
    TEST_ASSERT(memcmp(plat.storage, chunk0, sizeof(chunk0)) == 0);
    TEST_ASSERT(memcmp(plat.storage + sizeof(chunk0), chunk1, sizeof(chunk1)) == 0);
    return 0;
}

static int test_upload_invalid_offset_returns_expected(void) {
    struct fake_io io = {0};
    struct fake_platform plat = {0};
    struct smp_transport_io t = { .read = fake_read, .write = fake_write, .ctx = &io };
    struct smp_platform_ops p = { fake_slot_begin, fake_slot_write, fake_slot_finalize, fake_set_pending, &plat };
    struct smp_ctx ctx;
    uint8_t req[64];
    const uint8_t chunk0[] = {0x11, 0x22, 0x33};
    const uint8_t chunk_bad[] = {0xaa};
    size_t n;

    TEST_ASSERT(smp_init(&ctx, &t, &p, NULL) == 0);
    n = make_upload_req(req, 1u, 0u, chunk0, (uint16_t)sizeof(chunk0), (uint32_t)sizeof(chunk0));
    TEST_ASSERT(run_upload(&ctx, &io, req, n) == 0);

    n = make_upload_req(req, 2u, 99u, chunk_bad, (uint16_t)sizeof(chunk_bad), 0u);
    TEST_ASSERT(run_upload(&ctx, &io, req, n) == 0);
    TEST_ASSERT(resp_rc(io.tx) == SMP_ERR_INVALID_OFFSET);
    TEST_ASSERT(resp_expected_off(io.tx) == sizeof(chunk0));
    return 0;
}

static int test_upload_finalize_failure_maps_io_error(void) {
    struct fake_io io = {0};
    struct fake_platform plat = {0};
    struct smp_transport_io t = { .read = fake_read, .write = fake_write, .ctx = &io };
    struct smp_platform_ops p = { fake_slot_begin, fake_slot_write, fake_slot_finalize, fake_set_pending, &plat };
    struct smp_ctx ctx;
    uint8_t req[64];
    const uint8_t chunk0[] = {0x11};
    size_t n;

    plat.fail_finalize = 1;
    TEST_ASSERT(smp_init(&ctx, &t, &p, NULL) == 0);

    n = make_upload_req(req, 1u, 0u, chunk0, (uint16_t)sizeof(chunk0), (uint32_t)sizeof(chunk0));
    TEST_ASSERT(run_upload(&ctx, &io, req, n) == 0);
    TEST_ASSERT(resp_rc(io.tx) == SMP_ERR_IO);
    return 0;
}

static int test_unknown_command_maps_not_supported(void) {
    struct fake_io io = {0};
    struct fake_platform plat = {0};
    struct smp_transport_io t = { .read = fake_read, .write = fake_write, .ctx = &io };
    struct smp_platform_ops p = { fake_slot_begin, fake_slot_write, fake_slot_finalize, fake_set_pending, &plat };
    struct smp_ctx ctx;
    uint8_t frame[SMP_HEADER_SIZE] = {2u, 0u, 0u, 0u, 0u, 99u, 0u, 77u};

    TEST_ASSERT(smp_init(&ctx, &t, &p, NULL) == 0);
    TEST_ASSERT(run_upload(&ctx, &io, frame, sizeof(frame)) == 0);
    TEST_ASSERT(resp_rc(io.tx) == SMP_ERR_NOT_SUPPORTED);
    return 0;
}

static int test_fragmented_read_and_partial_write(void) {
    struct fake_io io = {0};
    struct fake_platform plat = {0};
    struct smp_transport_io t = { .read = fake_read, .write = fake_write, .ctx = &io };
    struct smp_platform_ops p = { fake_slot_begin, fake_slot_write, fake_slot_finalize, fake_set_pending, &plat };
    struct smp_ctx ctx;
    uint8_t req[64];
    const uint8_t chunk0[] = {0x10, 0x20, 0x30, 0x40};
    size_t n;

    io.max_read_chunk = 3u;
    io.max_write_chunk = 5u;

    TEST_ASSERT(smp_init(&ctx, &t, &p, NULL) == 0);
    n = make_upload_req(req, 1u, 0u, chunk0, (uint16_t)sizeof(chunk0), (uint32_t)sizeof(chunk0));
    TEST_ASSERT(run_upload(&ctx, &io, req, n) == 0);
    TEST_ASSERT(resp_rc(io.tx) == SMP_ERR_OK);
    TEST_ASSERT(resp_expected_off(io.tx) == sizeof(chunk0));
    return 0;
}

int main(void) {
    struct {
        const char *name;
        test_fn fn;
    } tests[] = {
        {"api_contract", test_api_contract},
        {"upload_happy_path", test_upload_happy_path},
        {"invalid_offset", test_upload_invalid_offset_returns_expected},
        {"finalize_failure", test_upload_finalize_failure_maps_io_error},
        {"unknown_command", test_unknown_command_maps_not_supported},
        {"fragmented_read_partial_write", test_fragmented_read_and_partial_write},
    };
    size_t i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        int rc = tests[i].fn();
        if (rc != 0) {
            printf("FAIL %s line %d\n", tests[i].name, rc);
            return 1;
        }
    }

    printf("PASS %zu tests\n", sizeof(tests) / sizeof(tests[0]));
    return 0;
}
