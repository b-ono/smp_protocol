#ifndef SMP_TARGET_H
#define SMP_TARGET_H

#include <stdint.h>
#include <stddef.h>

struct smp_target_config {
    uint32_t flash_address;
    uint32_t flash_size;
    uint32_t page_size;
    void *hal_flash_ctx;
};

int smp_target_init(const struct smp_target_config *cfg);
void smp_target_poll(void);
void smp_target_reset_upload(void);
enum smp_err smp_target_get_last_error(void);

#endif
