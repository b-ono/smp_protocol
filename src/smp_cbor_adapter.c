#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <cbor.h>

#include "smp_cbor_adapter.h"

static int read_u32(CborValue *value, uint32_t *out) {
    uint64_t v = 0;

    if (!cbor_value_is_integer(value) || cbor_value_is_negative_integer(value)) {
        return -1;
    }
    if (cbor_value_get_uint64(value, &v) != CborNoError || v > UINT32_MAX) {
        return -1;
    }
    *out = (uint32_t)v;
    return (cbor_value_advance_fixed(value) == CborNoError) ? 0 : -1;
}

static int read_bool(CborValue *value, uint8_t *out) {
    bool b = false;

    if (!cbor_value_is_boolean(value)) {
        return -1;
    }
    if (cbor_value_get_boolean(value, &b) != CborNoError) {
        return -1;
    }
    *out = b ? 1u : 0u;
    return (cbor_value_advance_fixed(value) == CborNoError) ? 0 : -1;
}

static int read_bstr(CborValue *value, uint8_t **out_data, size_t *out_len) {
    CborError err;
    size_t len = 0;
    uint8_t *buf = NULL;

    if (!cbor_value_is_byte_string(value)) {
        return -1;
    }

    err = cbor_value_dup_byte_string(value, &buf, &len, value);
    if (err != CborNoError) {
        return -1;
    }

    *out_data = buf;
    *out_len = len;
    return 0;
}

int smp_cbor_decode_img_upload_req(const uint8_t *payload,
                                   size_t payload_len,
                                   struct smp_img_upload_req *out) {
    CborParser parser;
    CborValue root;
    CborValue map;
    CborError err;

    if (!payload || !out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    err = cbor_parser_init(payload, payload_len, 0, &parser, &root);
    if (err != CborNoError || !cbor_value_is_map(&root)) {
        return -1;
    }

    err = cbor_value_enter_container(&root, &map);
    if (err != CborNoError) {
        return -1;
    }

    while (!cbor_value_at_end(&map)) {
        char key[16];
        size_t key_len = sizeof(key) - 1u;
        CborValue next;

        if (!cbor_value_is_text_string(&map)) {
            smp_cbor_free_img_upload_req(out);
            return -1;
        }

        err = cbor_value_copy_text_string(&map, key, &key_len, &next);
        if (err != CborNoError) {
            smp_cbor_free_img_upload_req(out);
            return -1;
        }
        key[key_len] = '\0';
        map = next;

        if (strcmp(key, "off") == 0) {
            if (read_u32(&map, &out->off) != 0) {
                smp_cbor_free_img_upload_req(out);
                return -1;
            }
        } else if (strcmp(key, "data") == 0) {
            if (read_bstr(&map, &out->data, &out->data_len) != 0) {
                smp_cbor_free_img_upload_req(out);
                return -1;
            }
        } else if (strcmp(key, "len") == 0) {
            out->has_len = 1u;
            if (read_u32(&map, &out->len) != 0) {
                smp_cbor_free_img_upload_req(out);
                return -1;
            }
        } else if (strcmp(key, "sha") == 0) {
            out->has_sha = 1u;
            if (read_bstr(&map, &out->sha, &out->sha_len) != 0) {
                smp_cbor_free_img_upload_req(out);
                return -1;
            }
        } else if (strcmp(key, "image") == 0) {
            out->has_image = 1u;
            if (read_u32(&map, &out->image) != 0) {
                smp_cbor_free_img_upload_req(out);
                return -1;
            }
        } else if (strcmp(key, "upgrade") == 0) {
            out->has_upgrade = 1u;
            if (read_bool(&map, &out->upgrade) != 0) {
                smp_cbor_free_img_upload_req(out);
                return -1;
            }
        } else {
            if (cbor_value_advance(&map) != CborNoError) {
                smp_cbor_free_img_upload_req(out);
                return -1;
            }
        }
    }

    if (!out->data && out->off != 0u) {
        return -1;
    }

    return 0;
}

void smp_cbor_free_img_upload_req(struct smp_img_upload_req *req) {
    if (!req) {
        return;
    }

    free(req->data);
    free(req->sha);
    req->data = NULL;
    req->sha = NULL;
    req->data_len = 0u;
    req->sha_len = 0u;
}

int smp_cbor_encode_img_upload_rsp(const struct smp_img_upload_rsp *rsp,
                                   uint8_t *out,
                                   size_t out_capacity,
                                   size_t *out_len) {
    CborEncoder encoder;
    CborEncoder map;
    CborError err;
    size_t pairs = rsp->has_match ? 3u : 2u;

    if (!rsp || !out || !out_len) {
        return -1;
    }

    cbor_encoder_init(&encoder, out, out_capacity, 0);
    err = cbor_encoder_create_map(&encoder, &map, pairs);
    if (err != CborNoError) {
        return -1;
    }

    err = cbor_encode_text_stringz(&map, "rc");
    if (err == CborNoError) {
        err = cbor_encode_int(&map, (int)rsp->rc);
    }
    if (err == CborNoError) {
        err = cbor_encode_text_stringz(&map, "off");
    }
    if (err == CborNoError) {
        err = cbor_encode_uint(&map, rsp->off);
    }

    if (err == CborNoError && rsp->has_match) {
        err = cbor_encode_text_stringz(&map, "match");
        if (err == CborNoError) {
            err = cbor_encode_boolean(&map, rsp->match != 0u);
        }
    }

    if (err == CborNoError) {
        err = cbor_encoder_close_container_checked(&encoder, &map);
    }
    if (err != CborNoError) {
        return -1;
    }

    *out_len = cbor_encoder_get_buffer_size(&encoder, out);
    return 0;
}

int smp_cbor_encode_rc_only(enum smp_err rc,
                            uint8_t *out,
                            size_t out_capacity,
                            size_t *out_len) {
    struct smp_img_upload_rsp rsp;

    rsp.rc = rc;
    rsp.off = 0u;
    rsp.has_match = 0u;
    rsp.match = 0u;

    return smp_cbor_encode_img_upload_rsp(&rsp, out, out_capacity, out_len);
}
