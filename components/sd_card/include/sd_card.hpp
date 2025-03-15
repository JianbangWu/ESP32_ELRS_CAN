#pragma once

#include <string>
#include <vector>
#include "dirent.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "cJSON.h"

#include "ds3231m.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

    class SDCard
    {
    private:
        const char *TAG = "SDCard";
        static const size_t MAX_FILE_SIZE = 10 * 1024 * 1024; // 10MB
        const std::string mt{"/sdcard"};

        /* Init Parameter */
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

        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

        sdmmc_card_t *card;

#ifdef CONFIG_ENABLE_DETECT_FEATURE
        const gpio_num_t detect_pin = (gpio_num_t)CONFIG_PIN_DET;
#endif

    public:
        SDCard(/* args */);
        ~SDCard();
        void mount_sd(void);
        void unmount_sd(void);
        void format_sd(void);

        static bool isFileSizeExceeded(std::string filename);

        static bool writeFile(const std::string &filename, const std::vector<uint8_t> &data);

        void read_user_data_to_serial(const char *user_data_filename);

        static std::vector<std::string> getFileList(void);

        static std::vector<std::string> getFileList(const std::string &directory);

        bool write_json_to_file(const char *filename, cJSON *json);
        cJSON *read_json_from_file(const char *filename);
    };

#ifdef __cplusplus
}
#endif
