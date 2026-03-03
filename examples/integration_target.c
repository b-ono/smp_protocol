#include "integration_target.h"
#include "smp/smp.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

struct smp_target_ctx {
    struct smp_ctx smp;
    struct smp_transport_io transport;
    struct smp_platform_ops platform;
    struct smp_target_config config;
    enum smp_err last_error;
};

static struct smp_target_ctx g_ctx;

struct flash_ctx {
    uint32_t address;
    uint32_t size;
    uint32_t page_size;
};

static int flash_begin(void *ctx) {
    struct flash_ctx *flash = (struct flash_ctx *)ctx;
    (void)flash;
    return 0;
}

static int flash_write(void *ctx, uint32_t off, const uint8_t *data, size_t len) {
    struct flash_ctx *flash = (struct flash_ctx *)ctx;
    uint32_t addr = flash->address + off;
    (void)addr;
    (void)data;
    (void)len;
    return 0;
}

static int flash_finalize(void *ctx, size_t total_size) {
    struct flash_ctx *flash = (struct flash_ctx *)ctx;
    (void)flash;
    (void)total_size;
    return 0;
}

static int set_pending(void *ctx) {
    struct flash_ctx *flash = (struct flash_ctx *)ctx;
    (void)flash;
    return 0;
}

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

int smp_target_init(const struct smp_target_config *cfg) {
    struct flash_ctx flash_ctx;

    memset(&g_ctx, 0, sizeof(g_ctx));
    memcpy(&g_ctx.config, cfg, sizeof(*cfg));

    flash_ctx.address = cfg->flash_address;
    flash_ctx.size = cfg->flash_size;
    flash_ctx.page_size = cfg->page_size;

    g_ctx.transport.read = transport_read;
    g_ctx.transport.write = transport_write;
    g_ctx.transport.ctx = NULL;

    g_ctx.platform.slot_begin = flash_begin;
    g_ctx.platform.slot_write = flash_write;
    g_ctx.platform.slot_finalize = flash_finalize;
    g_ctx.platform.set_pending = set_pending;
    g_ctx.platform.ctx = &flash_ctx;

    if (smp_init(&g_ctx.smp, &g_ctx.transport, &g_ctx.platform, NULL) != 0) {
        return -1;
    }

    g_ctx.last_error = SMP_ERR_OK;
    return 0;
}

void smp_target_poll(void) {
    if (smp_poll(&g_ctx.smp) != 0) {
        g_ctx.last_error = SMP_ERR_IO;
    }
}

void smp_target_reset_upload(void) {
    smp_reset_upload_state(&g_ctx.smp);
}

enum smp_err smp_target_get_last_error(void) {
    return g_ctx.last_error;
}

int main(void) {
    struct smp_target_config config = {
        .flash_address = 0x08000000,
        .flash_size = 512 * 1024,
        .page_size = 4096,
        .hal_flash_ctx = NULL,
    };

    if (smp_target_init(&config) != 0) {
        return 1;
    }

    printf("SMP target initialized\n");
    printf("Flash address: 0x%08X\n", config.flash_address);
    printf("Flash size: %u bytes\n", config.flash_size);
    printf("Page size: %u bytes\n", config.page_size);

    for (;;) {
        smp_target_poll();
    }

    return 0;
}
