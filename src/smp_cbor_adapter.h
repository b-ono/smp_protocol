#ifndef SMP_CBOR_ADAPTER_H
#define SMP_CBOR_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#include "smp/smp_types.h"

struct smp_img_upload_req {
    uint32_t off;
    uint8_t *data;
    size_t data_len;

    uint8_t has_len;
    uint32_t len;

    uint8_t has_sha;
    uint8_t *sha;
    size_t sha_len;

    uint8_t has_image;
    uint32_t image;

    uint8_t has_upgrade;
    uint8_t upgrade;
};

struct smp_img_upload_rsp {
    enum smp_err rc;
    uint32_t off;
    uint8_t has_match;
    uint8_t match;
};

int smp_cbor_decode_img_upload_req(const uint8_t *payload,
                                   size_t payload_len,
                                   struct smp_img_upload_req *out);

void smp_cbor_free_img_upload_req(struct smp_img_upload_req *req);

int smp_cbor_encode_img_upload_rsp(const struct smp_img_upload_rsp *rsp,
                                   uint8_t *out,
                                   size_t out_capacity,
                                   size_t *out_len);

int smp_cbor_encode_rc_only(enum smp_err rc,
                            uint8_t *out,
                            size_t out_capacity,
                            size_t *out_len);

#endif
