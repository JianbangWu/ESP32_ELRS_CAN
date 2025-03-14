#pragma once

#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "cJSON.h"

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
            .allocation_unit_size = 16 * 1024,
            .disk_status_check_enable = 1,
            .use_one_fat = false,
        };
        const char mount_point[10]{"/sdcard"};
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

#ifdef CONFIG_ENABLE_DETECT_FEATURE
        const gpio_num_t detect_pin = (gpio_num_t)CONFIG_PIN_DET;
#endif
        FILE *log_file;
        FILE *user_data_file;

    public:
        sdmmc_card_t *card;
        sd_card(/* args */);
        ~sd_card();
        void mount_sd(void);
        void unmount_sd(void);
        void format_sd(void);
        bool open_log_file(const char *log_filename);
        void write_log(const char *log_message);
        void close_log_file();
        bool open_user_data_file(const char *user_data_filename);
        void write_user_data(const char *data);
        void close_user_data_file();
        void read_log_to_serial(const char *log_filename);
        void read_user_data_to_serial(const char *user_data_filename);

        bool write_json_to_file(const char *filename, cJSON *json);
        cJSON *read_json_from_file(const char *filename);
    };

#ifdef __cplusplus
}
#endif
