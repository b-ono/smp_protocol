#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <stddef.h>
#include <stdint.h>

#include "smp/smp.h"

#define TEST_ASSERT(expr) do { if (!(expr)) return __LINE__; } while (0)

struct fake_io {
    uint8_t rx[4096];
    size_t rx_len;
    size_t rx_off;
    size_t max_read_chunk;

    uint8_t tx[4096];
    size_t tx_len;
    size_t max_write_chunk;
    int write_fail;
};

struct fake_platform {
    uint8_t storage[4096];
    size_t write_calls;
    size_t begin_calls;
    size_t finalize_calls;
    size_t pending_calls;
    int fail_begin;
    int fail_write;
    int fail_finalize;
    int fail_pending;
    size_t finalized_total;
};

int fake_read(void *ctx, uint8_t *buf, size_t max_len);
int fake_write(void *ctx, const uint8_t *buf, size_t len);
int fake_slot_begin(void *ctx);
int fake_slot_write(void *ctx, uint32_t off, const uint8_t *data, size_t len);
int fake_slot_finalize(void *ctx, size_t total_size);
int fake_set_pending(void *ctx);

size_t make_upload_req(uint8_t *out,
                       uint8_t seq,
                       uint32_t off,
                       const uint8_t *data,
                       uint16_t data_len,
                       uint32_t total_len);

uint16_t resp_rc(const uint8_t *frame);
uint32_t resp_expected_off(const uint8_t *frame);

#endif
