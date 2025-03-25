#include <sys/stat.h>
#include <stdint.h>
#include <sys/unistd.h>
#include <errno.h>

#include "sd_card.hpp"
#include "esp_log.h"
#include "esp_vfs_fat.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const uint32_t StackSize = 1024 * 5;

void IRAM_ATTR SDCard::gpio_isr_handler(void *arg)
{
    SDCard *instance = static_cast<SDCard *>(arg);
    xSemaphoreGiveFromISR(instance->det_sem, nullptr);
}

void SDCard::card_detect()
{
    while (1)
    {
        if (xSemaphoreTake(det_sem, pdMS_TO_TICKS(300)))
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            if (gpio_get_level(_det_pin) == 1)
            {
                ESP_LOGI(TAG, "SD Card is Plug-In!");
                mount_sd();
            }
            else
            {
                ESP_LOGI(TAG, "SD Card is Plug-Out!");
                unmount_sd();
            }
        }
    }
}

SDCard::SDCard(gpio_num_t clk_pin,
               gpio_num_t cmd_pin,
               gpio_num_t d0_pin,
               gpio_num_t d1_pin,
               gpio_num_t d2_pin,
               gpio_num_t d3_pin,
               gpio_num_t det_pin) : _clk_pin(clk_pin), _cmd_pin(cmd_pin), _d0_pin(d0_pin), _d1_pin(d1_pin), _d2_pin(d2_pin), _d3_pin(d3_pin), _det_pin(det_pin)
{

    gpio_config_t io_conf = {};
    /* RST_PIN */
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << det_pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    // sd_evt_queue = xQueueCreate(1, sizeof(uint32_t));
    det_sem = xSemaphoreCreateBinary(); // 创建二进制信号量

    auto task_func = [](void *arg)
    {
        SDCard *instance = static_cast<SDCard *>(arg);
        instance->card_detect(); // 调用类的成员函数
    };

    xTaskCreate(task_func, "sd_isr", StackSize, this, 10, NULL);

    esp_err_t ret = gpio_install_isr_service(0);

    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Detect Pin ISR Init Failed! 0x%x", ret);
        return;
    }

    if (ESP_OK == (gpio_isr_handler_add(det_pin, gpio_isr_handler, this)))
    {
        ESP_LOGI(TAG, "ISR install Success");
    }

    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SDMMC peripheral");

    _host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    _slot_config.width = 4;

    _slot_config.clk = _clk_pin;
    _slot_config.cmd = _cmd_pin;
    _slot_config.d0 = _d0_pin;
    _slot_config.d1 = _d1_pin;
    _slot_config.d2 = _d2_pin;
    _slot_config.d3 = _d3_pin;

    _slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    if (gpio_get_level(det_pin) == 1)
    {
        ESP_LOGI(TAG, "SD-Card Detected!");
    }
    else
    {
        ESP_LOGI(TAG, "No SD-Card Detected!");
        return;
    }
    mount_sd();
}

SDCard::~SDCard()
{
    unmount_sd();
}

void SDCard::mount_sd(void)
{
    ESP_LOGI(TAG, "Mounting filesystem");

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(_mount_point.c_str(), &_host, &_slot_config, &mount_config, &_card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }

    ESP_LOGI(TAG, "SD Card mounted at: %s", _mount_point.c_str());

    // sdmmc_card_print_info(stdout, card);

    struct stat st;
    if (stat("/sdcard", &st) != 0)
    {
        ESP_LOGE(TAG, "SD Card mount point does not exist!");
    }
}

void SDCard::unmount_sd(void)
{
    esp_vfs_fat_sdcard_unmount(_mount_point.c_str(), _card);
    ESP_LOGI(TAG, "Card unmounted");
}

void SDCard::format_sd(void)
{
    esp_err_t err = esp_vfs_fat_sdcard_format_cfg(_mount_point.c_str(), _card, &mount_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Card Format Err := %x", err);
    }
}

std::string SDCard::get_mount_path(void)
{
    return _mount_point;
}

void SDCard::set_card_handle(sdmmc_card_t *card)
{
    _card = card;
}

sdmmc_card_t *&SDCard::get_card_handle(void)
{
    return _card;
}

sdmmc_host_t &SDCard::get_host_handle(void)
{
    return _host;
}

sdmmc_slot_config_t &SDCard::get_slot_config(void)
{
    return _slot_config;
}
