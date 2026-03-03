#ifndef SMP_INTERNAL_H
#define SMP_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "smp/smp_types.h"
#include "smp_cbor_adapter.h"

struct smp_req_view {
    struct smp_header header;
    const uint8_t *payload;
    size_t payload_len;
};

int smp_parse_header(const uint8_t *buf, size_t len, struct smp_header *out);

int smp_dispatch_request(struct smp_ctx *ctx,
                         const struct smp_req_view *req,
                         uint8_t *resp_payload,
                         size_t resp_capacity,
                         size_t *resp_len,
                         enum smp_err *err);

int smp_handle_img_upload(struct smp_ctx *ctx,
                          const struct smp_req_view *req,
                          uint8_t *resp_payload,
                          size_t resp_capacity,
                          size_t *resp_len,
                          enum smp_err *err);

#endif
