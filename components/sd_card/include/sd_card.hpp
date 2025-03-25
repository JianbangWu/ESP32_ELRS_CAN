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

        static const size_t MAX_FILE_SIZE = 1 * 512 * 1024; // 10MB

        const std::string _mount_point{"/sdcard"};

        sdmmc_card_t *_card = nullptr;
        sdmmc_host_t _host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t _slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

        const gpio_num_t _clk_pin;
        const gpio_num_t _cmd_pin;
        const gpio_num_t _d0_pin;
        const gpio_num_t _d1_pin;
        const gpio_num_t _d2_pin;
        const gpio_num_t _d3_pin;
        const gpio_num_t _det_pin;

        static void IRAM_ATTR gpio_isr_handler(void *arg);

        /* Init Parameter */
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024,
            .disk_status_check_enable = 1,
            .use_one_fat = false,
        };

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

        std::string get_mount_path(void);
        void set_card_handle(sdmmc_card_t *card);
        sdmmc_card_t *&get_card_handle(void);
        sdmmc_host_t &get_host_handle(void);
        sdmmc_slot_config_t &get_slot_config(void);
    };

#ifdef __cplusplus
}
#endif
