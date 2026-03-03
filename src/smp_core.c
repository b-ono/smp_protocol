#include <string.h>

#include "smp/smp.h"
#include "smp_internal.h"

static uint16_t be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void put_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)((v >> 8) & 0xffu);
    p[1] = (uint8_t)(v & 0xffu);
}

static size_t frame_size_from_rx(const uint8_t *buf, size_t rx_len) {
    if (rx_len < SMP_HEADER_SIZE) {
        return 0u;
    }
    return (size_t)SMP_HEADER_SIZE + (size_t)be16(buf + 2);
}

int smp_parse_header(const uint8_t *buf, size_t len, struct smp_header *out) {
    if (!buf || !out || len < SMP_HEADER_SIZE) {
        return -1;
    }

    out->op = buf[0];
    out->flags = buf[1];
    out->len = be16(buf + 2);
    out->group = be16(buf + 4);
    out->seq = buf[6];
    out->id = buf[7];
    return 0;
}

static int flush_tx(struct smp_ctx *ctx) {
    int wrote;
    size_t remaining;

    if (ctx->tx_off >= ctx->tx_len) {
        ctx->tx_off = 0u;
        ctx->tx_len = 0u;
        return 0;
    }

    remaining = ctx->tx_len - ctx->tx_off;
    wrote = ctx->io.write(ctx->io.ctx, ctx->tx_buf + ctx->tx_off, remaining);
    if (wrote < 0) {
        ctx->last_error = SMP_ERR_IO;
        return -1;
    }
    if (wrote == 0) {
        return 0;
    }

    ctx->tx_off += (size_t)wrote;
    if (ctx->tx_off >= ctx->tx_len) {
        ctx->tx_off = 0u;
        ctx->tx_len = 0u;
    }
    return 0;
}

static int process_one_frame(struct smp_ctx *ctx, const uint8_t *frame, size_t frame_size) {
    struct smp_req_view req;
    size_t resp_payload_len = 0u;
    enum smp_err err = SMP_ERR_OK;
    int rc;

    (void)frame_size;

    if (smp_parse_header(frame, SMP_HEADER_SIZE, &req.header) != 0) {
        ctx->last_error = SMP_ERR_BAD_FRAME;
        return -1;
    }

    req.payload = frame + SMP_HEADER_SIZE;
    req.payload_len = req.header.len;

    rc = smp_dispatch_request(ctx,
                              &req,
                              ctx->tx_buf + SMP_HEADER_SIZE,
                              ctx->tx_buf_size - SMP_HEADER_SIZE,
                              &resp_payload_len,
                              &err);

    if (rc != 0) {
        ctx->last_error = err;
        return -1;
    }

    if (resp_payload_len > (ctx->tx_buf_size - SMP_HEADER_SIZE)) {
        ctx->last_error = SMP_ERR_BAD_FRAME;
        return -1;
    }

    if (err != SMP_ERR_OK && resp_payload_len == 0u) {
        if (smp_cbor_encode_rc_only(err,
                                    ctx->tx_buf + SMP_HEADER_SIZE,
                                    ctx->tx_buf_size - SMP_HEADER_SIZE,
                                    &resp_payload_len) != 0) {
            ctx->last_error = SMP_ERR_BAD_FRAME;
            return -1;
        }
    }

    ctx->tx_buf[0] = (uint8_t)(req.header.op | 0x01u);
    ctx->tx_buf[1] = 0u;
    put_be16(ctx->tx_buf + 2, (uint16_t)resp_payload_len);
    put_be16(ctx->tx_buf + 4, req.header.group);
    ctx->tx_buf[6] = req.header.seq;
    ctx->tx_buf[7] = req.header.id;
    ctx->tx_len = SMP_HEADER_SIZE + resp_payload_len;
    ctx->tx_off = 0u;
    ctx->last_error = err;
    return 0;
}

int smp_init(struct smp_ctx *ctx,
             const struct smp_transport_io *io,
             const struct smp_platform_ops *ops,
             const struct smp_cfg *cfg) {
    if (!ctx || !io || !io->read || !io->write || !ops || !ops->slot_write) {
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->io = *io;
    ctx->ops = *ops;
    ctx->rx_buf_size = cfg && cfg->rx_buf_size ? cfg->rx_buf_size : sizeof(ctx->rx_buf);
    ctx->tx_buf_size = cfg && cfg->tx_buf_size ? cfg->tx_buf_size : sizeof(ctx->tx_buf);

    if (ctx->rx_buf_size > sizeof(ctx->rx_buf) || ctx->tx_buf_size > sizeof(ctx->tx_buf)) {
        return -1;
    }

    return 0;
}

void smp_reset_upload_state(struct smp_ctx *ctx) {
    if (!ctx) {
        return;
    }
    ctx->expected_off = 0u;
    ctx->bytes_written = 0u;
    ctx->upload_active = 0u;
    ctx->upload_complete = 0u;
    ctx->total_size = 0u;
    ctx->total_size_known = 0u;
    ctx->last_error = SMP_ERR_OK;
}

int smp_poll(struct smp_ctx *ctx) {
    int rd;
    size_t frame_size;

    if (!ctx) {
        return -1;
    }

    if (flush_tx(ctx) != 0) {
        return -1;
    }

    if (ctx->rx_len < ctx->rx_buf_size) {
        rd = ctx->io.read(ctx->io.ctx,
                          ctx->rx_buf + ctx->rx_len,
                          ctx->rx_buf_size - ctx->rx_len);
        if (rd < 0) {
            ctx->last_error = SMP_ERR_IO;
            return -1;
        }
        ctx->rx_len += (size_t)rd;
    }

    while (ctx->tx_len == 0u) {
        frame_size = frame_size_from_rx(ctx->rx_buf, ctx->rx_len);
        if (frame_size == 0u) {
            break;
        }
        if (frame_size > ctx->rx_buf_size) {
            ctx->last_error = SMP_ERR_BAD_FRAME;
            return -1;
        }
        if (frame_size > ctx->rx_len) {
            break;
        }

        if (process_one_frame(ctx, ctx->rx_buf, frame_size) != 0) {
            return -1;
        }

        memmove(ctx->rx_buf, ctx->rx_buf + frame_size, ctx->rx_len - frame_size);
        ctx->rx_len -= frame_size;
    }

    return 0;
}
