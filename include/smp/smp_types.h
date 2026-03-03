#ifndef SMP_TYPES_H
#define SMP_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMP_HEADER_SIZE 8u
#define SMP_IMAGE_GROUP_ID 1u
#define SMP_IMAGE_UPLOAD_ID 1u

typedef int (*smp_read_fn)(void *ctx, uint8_t *buf, size_t max_len);
typedef int (*smp_write_fn)(void *ctx, const uint8_t *buf, size_t len);

typedef int (*smp_slot_begin_fn)(void *ctx);
typedef int (*smp_slot_write_fn)(void *ctx, uint32_t off, const uint8_t *data, size_t len);
typedef int (*smp_slot_finalize_fn)(void *ctx, size_t total_size);
typedef int (*smp_set_pending_fn)(void *ctx);

enum smp_err {
    SMP_ERR_OK = 0,
    SMP_ERR_BAD_FRAME = 1,
    SMP_ERR_NOT_SUPPORTED = 2,
    SMP_ERR_INVALID_OFFSET = 3,
    SMP_ERR_IO = 4,
    SMP_ERR_BUSY = 5,
    SMP_ERR_STATE = 6
};

struct smp_cfg {
    size_t rx_buf_size;
    size_t tx_buf_size;
};

struct smp_transport_io {
    smp_read_fn read;
    smp_write_fn write;
    void *ctx;
};

struct smp_platform_ops {
    smp_slot_begin_fn slot_begin;
    smp_slot_write_fn slot_write;
    smp_slot_finalize_fn slot_finalize;
    smp_set_pending_fn set_pending;
    void *ctx;
};

struct smp_header {
    uint8_t op;
    uint8_t flags;
    uint16_t len;
    uint16_t group;
    uint8_t seq;
    uint8_t id;
};

struct smp_ctx {
    struct smp_transport_io io;
    struct smp_platform_ops ops;

    size_t rx_buf_size;
    size_t tx_buf_size;
    uint8_t rx_buf[1024];
    uint8_t tx_buf[1024];

    size_t rx_len;
    size_t tx_len;
    size_t tx_off;

    uint32_t expected_off;
    uint32_t bytes_written;
    uint32_t total_size;
    uint8_t upload_active;
    uint8_t upload_complete;
    uint8_t total_size_known;
    enum smp_err last_error;
};

#ifdef __cplusplus
}
#endif

#endif
