#include "sd_card.hpp"

#include <stdint.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static QueueHandle_t sd_evt_queue = NULL;
#include "beep.hpp"

extern Buzzer *buzzer_obj;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(sd_evt_queue, &gpio_num, NULL);
}

static void card_detect(void *arg)
{
    class sd_card *phandle = (class sd_card *)arg;
    gpio_num_t io_num;
    for (;;)
    {
        if (xQueueReceive(sd_evt_queue, &io_num, portMAX_DELAY))
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            if (gpio_get_level(io_num) == 1)
            {
                buzzer_obj->Beep(4000, 70);
                vTaskDelay(pdMS_TO_TICKS(30));
                buzzer_obj->Beep(4000, 70);
                printf("SD Card is Plug-In! \r\n");
                phandle->mount_sd();
            }
            else
            {
                buzzer_obj->Beep(4000, 100);
                printf("SD Card is Plug-Out! \r\n");
                phandle->unmount_sd();
            }
        }
    }
}

sd_card::sd_card(/* args */)
{
#ifdef CONFIG_ENABLE_DETECT_FEATURE
    gpio_config_t io_conf = {};
    /* RST_PIN */
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << CONFIG_PIN_DET);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    sd_evt_queue = xQueueCreate(1, sizeof(uint32_t));

    xTaskCreate(card_detect, "sd_int", 1024 * 5, this, 10, NULL);

    esp_err_t ret = gpio_install_isr_service(0);

    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Detect Pin ISR Init Failed! 0x%x", ret);
        return;
    }
    gpio_isr_handler_add((gpio_num_t)CONFIG_PIN_DET, gpio_isr_handler, (void *)CONFIG_PIN_DET);
#endif

    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SDMMC peripheral");

#if CONFIG_SDMMC_SPEED_HS
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
#elif CONFIG_SDMMC_SPEED_UHS_I_SDR50
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_SDR50;
    host.flags &= ~SDMMC_HOST_FLAG_DDR;
#elif CONFIG_SDMMC_SPEED_UHS_I_DDR50
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_DDR50;
#endif

#if IS_UHS1
    slot_config.flags |= SDMMC_SLOT_FLAG_UHS1;
#endif

#ifdef CONFIG_SDMMC_BUS_WIDTH_4
    slot_config.width = 4;
#else
    slot_config.width = 1;
#endif

#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = (gpio_num_t)CONFIG_PIN_CLK;
    slot_config.cmd = (gpio_num_t)CONFIG_PIN_CMD;
    slot_config.d0 = (gpio_num_t)CONFIG_PIN_D0;
#ifdef CONFIG_SDMMC_BUS_WIDTH_4
    slot_config.d1 = (gpio_num_t)CONFIG_PIN_D1;
    slot_config.d2 = (gpio_num_t)CONFIG_PIN_D2;
    slot_config.d3 = (gpio_num_t)CONFIG_PIN_D3;
#endif // CONFIG_SDMMC_BUS_WIDTH_4
#endif // CONFIG_SOC_SDMMC_USE_GPIO_MATRIX

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

#ifdef CONFIG_ENABLE_DETECT_FEATURE
    if (gpio_get_level((gpio_num_t)CONFIG_PIN_DET) == 1)
    {
        ESP_LOGI(TAG, "SD-Card Detected!");
    }
    else
    {
        ESP_LOGI(TAG, "No SD-Card Detected!");
        return;
    }
#endif
    mount_sd();
}

sd_card::~sd_card()
{
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");
}

void sd_card::mount_sd(void)
{
    ESP_LOGI(TAG, "Mounting filesystem");
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

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
#ifdef CONFIG_DEBUG_PIN_CONNECTIONS
            check_sd_card_pins(&config, pin_count);
#endif
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
}

void sd_card::unmount_sd(void)
{
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");
}