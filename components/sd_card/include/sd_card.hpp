#pragma once

#include <string>
#include <vector>

#ifdef __cplusplus
extern "C"
{
#endif

#include "dirent.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "cJSON.h"

    class SDCard
    {
    private:
        const char *TAG = "SDCard";

        SemaphoreHandle_t det_sem;

        static const size_t MAX_FILE_SIZE = 1 * 1024 * 1024; // 10MB

        const std::string mt{"/sdcard"};

        const gpio_num_t _clk_pin;
        const gpio_num_t _cmd_pin;
        const gpio_num_t _d0_pin;
        const gpio_num_t _d1_pin;
        const gpio_num_t _d2_pin;
        const gpio_num_t _d3_pin;
        const gpio_num_t _det_pin;

        static void IRAM_ATTR gpio_isr_handler(void *arg);

        sdmmc_card_t *card;

        /* Init Parameter */
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024,
            .disk_status_check_enable = 1,
            .use_one_fat = false,
        };
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

        void card_detect(void);

    public:
        SDCard(gpio_num_t clk_pin = GPIO_NUM_13,
               gpio_num_t cmd_pin = GPIO_NUM_14,
               gpio_num_t d0_pin = GPIO_NUM_12,
               gpio_num_t d1_pin = GPIO_NUM_11,
               gpio_num_t d2_pin = GPIO_NUM_47,
               gpio_num_t d3_pin = GPIO_NUM_21,
               gpio_num_t det_pin = GPIO_NUM_48);
        ~SDCard();

        void mount_sd(void);
        void unmount_sd(void);
        void format_sd(void);

        static bool isFileSizeExceeded(std::string filename);
        static bool writeFile(const std::string &filename, const std::vector<uint8_t> &data);

        void read_user_data_to_serial(const char *user_data_filename);

        static std::vector<std::string> getFileList(const std::string &directory);

        bool write_json_to_file(const char *filename, cJSON *json);
        cJSON *read_json_from_file(const char *filename);
    };

#ifdef __cplusplus
}
#endif
