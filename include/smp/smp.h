#ifndef SMP_H
#define SMP_H

#include "smp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int smp_init(struct smp_ctx *ctx,
             const struct smp_transport_io *io,
             const struct smp_platform_ops *ops,
             const struct smp_cfg *cfg);

int smp_poll(struct smp_ctx *ctx);

void smp_reset_upload_state(struct smp_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif
