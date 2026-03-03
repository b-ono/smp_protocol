#include <stdint.h>
#include <stdio.h>

#include "smp/smp.h"

static int transport_read(void *ctx, uint8_t *buf, size_t max_len) {
    (void)ctx;
    (void)buf;
    (void)max_len;
    return 0;
}

static int transport_write(void *ctx, const uint8_t *buf, size_t len) {
    (void)ctx;
    (void)buf;
    return (int)len;
}

static int slot_begin(void *ctx) {
    (void)ctx;
    return 0;
}

static int slot_write(void *ctx, uint32_t off, const uint8_t *data, size_t len) {
    (void)ctx;
    (void)off;
    (void)data;
    (void)len;
    return 0;
}

static int slot_finalize(void *ctx, size_t total_size) {
    (void)ctx;
    (void)total_size;
    return 0;
}

static int set_pending(void *ctx) {
    (void)ctx;
    return 0;
}

int main(void) {
    struct smp_ctx ctx;
    struct smp_transport_io io = {
        .read = transport_read,
        .write = transport_write,
        .ctx = NULL,
    };
    struct smp_platform_ops ops = {
        .slot_begin = slot_begin,
        .slot_write = slot_write,
        .slot_finalize = slot_finalize,
        .set_pending = set_pending,
        .ctx = NULL,
    };

    if (smp_init(&ctx, &io, &ops, NULL) != 0) {
        return 1;
    }

    for (;;) {
        if (smp_poll(&ctx) != 0) {
            printf("smp_poll error\n");
        }
        break;
    }

    return 0;
}
