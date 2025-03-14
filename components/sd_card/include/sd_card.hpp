#pragma once

#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"

#ifdef __cplusplus
extern "C"
{
#endif
    class sd_card
    {
    private:
        const char *TAG = "sd_card";
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_FORMAT_IF_MOUNT_FAILED
            .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
            .max_files = 5,
            .allocation_unit_size = 16 * 1024};
        sdmmc_card_t *card;
        const char mount_point[10]{"/sdcard"};
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

#ifdef CONFIG_ENABLE_DETECT_FEATURE
        const gpio_num_t detect_pin = (gpio_num_t)CONFIG_PIN_DET;
#endif

    public:
        sd_card(/* args */);
        ~sd_card();
        void mount_sd(void);
        void unmount_sd(void);
    };

#ifdef __cplusplus
}
#endif
