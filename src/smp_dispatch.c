#include "smp_internal.h"

int smp_dispatch_request(struct smp_ctx *ctx,
                         const struct smp_req_view *req,
                         uint8_t *resp_payload,
                         size_t resp_capacity,
                         size_t *resp_len,
                         enum smp_err *err) {
    if (req->header.group == SMP_IMAGE_GROUP_ID && req->header.id == SMP_IMAGE_UPLOAD_ID) {
        return smp_handle_img_upload(ctx, req, resp_payload, resp_capacity, resp_len, err);
    }

    *err = SMP_ERR_NOT_SUPPORTED;
    *resp_len = 0;
    return 0;
}
