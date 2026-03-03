#include "smp_internal.h"

static int encode_upload_resp(uint8_t *resp_payload,
                              size_t resp_capacity,
                              enum smp_err err,
                              uint32_t expected_off,
                              size_t *resp_len,
                              uint8_t has_match,
                              uint8_t match) {
    struct smp_img_upload_rsp rsp;

    rsp.rc = err;
    rsp.off = expected_off;
    rsp.has_match = has_match;
    rsp.match = match;
    return smp_cbor_encode_img_upload_rsp(&rsp, resp_payload, resp_capacity, resp_len);
}

int smp_handle_img_upload(struct smp_ctx *ctx,
                          const struct smp_req_view *req,
                          uint8_t *resp_payload,
                          size_t resp_capacity,
                          size_t *resp_len,
                          enum smp_err *err) {
    struct smp_img_upload_req upload;
    uint8_t has_match = 0u;

    if (smp_cbor_decode_img_upload_req(req->payload, req->payload_len, &upload) != 0) {
        *err = SMP_ERR_BAD_FRAME;
        return encode_upload_resp(resp_payload, resp_capacity, *err, ctx->expected_off, resp_len, 0u, 0u);
    }

    if (upload.off == 0u && !ctx->upload_active && ctx->expected_off == 0u) {
        if (ctx->ops.slot_begin && ctx->ops.slot_begin(ctx->ops.ctx) != 0) {
            *err = SMP_ERR_IO;
            ctx->last_error = *err;
            smp_cbor_free_img_upload_req(&upload);
            return encode_upload_resp(resp_payload, resp_capacity, *err, ctx->expected_off, resp_len, 0u, 0u);
        }
        ctx->upload_active = 1u;
    }

    if (upload.off != ctx->expected_off) {
        *err = SMP_ERR_INVALID_OFFSET;
        ctx->last_error = *err;
        has_match = 1u;
        smp_cbor_free_img_upload_req(&upload);
        return encode_upload_resp(resp_payload, resp_capacity, *err, ctx->expected_off, resp_len, has_match, 0u);
    }

    if (!ctx->ops.slot_write || ctx->ops.slot_write(ctx->ops.ctx, upload.off, upload.data, upload.data_len) != 0) {
        *err = SMP_ERR_IO;
        ctx->last_error = *err;
        smp_cbor_free_img_upload_req(&upload);
        return encode_upload_resp(resp_payload, resp_capacity, *err, ctx->expected_off, resp_len, 0u, 0u);
    }

    ctx->expected_off += (uint32_t)upload.data_len;
    ctx->bytes_written += (uint32_t)upload.data_len;

    if (upload.has_len) {
        ctx->total_size = upload.len;
        ctx->total_size_known = 1u;
    }

    if (ctx->total_size_known && ctx->expected_off >= ctx->total_size) {
        if (ctx->ops.slot_finalize && ctx->ops.slot_finalize(ctx->ops.ctx, ctx->bytes_written) != 0) {
            *err = SMP_ERR_IO;
            ctx->last_error = *err;
            smp_cbor_free_img_upload_req(&upload);
            return encode_upload_resp(resp_payload, resp_capacity, *err, ctx->expected_off, resp_len, 0u, 0u);
        }

        if (ctx->ops.set_pending && ctx->ops.set_pending(ctx->ops.ctx) != 0) {
            *err = SMP_ERR_IO;
            ctx->last_error = *err;
            smp_cbor_free_img_upload_req(&upload);
            return encode_upload_resp(resp_payload, resp_capacity, *err, ctx->expected_off, resp_len, 0u, 0u);
        }

        ctx->upload_complete = 1u;
        ctx->upload_active = 0u;
    }

    smp_cbor_free_img_upload_req(&upload);
    *err = SMP_ERR_OK;
    ctx->last_error = *err;
    return encode_upload_resp(resp_payload, resp_capacity, *err, ctx->expected_off, resp_len, 0u, 0u);
}
