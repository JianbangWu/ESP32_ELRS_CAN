#include "sd_card.hpp"
#include "errno.h"
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
        }
        return;
    }

    ESP_LOGI(TAG, "SD Card mounted at: %s", mount_point);

    sdmmc_card_print_info(stdout, card);

    struct stat st;
    if (stat("/sdcard", &st) != 0)
    {
        ESP_LOGE(TAG, "SD Card mount point does not exist!");
    }
}

void sd_card::unmount_sd(void)
{
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");
}

void sd_card::format_sd(void)
{
    esp_err_t err = esp_vfs_fat_sdcard_format_cfg(mount_point, card, &mount_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Card Format Err := %x", err);
    }
}

bool sd_card::open_log_file(const char *log_filename)
{
    log_file = fopen(log_filename, "a");
    if (log_file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open log file");
        return false;
    }
    return true;
}

void sd_card::write_log(const char *log_message)
{
    if (log_file != NULL)
    {
        fprintf(log_file, "%s\n", log_message);
        fflush(log_file); // Ensure data is written to the file
    }
}

void sd_card::close_log_file()
{
    if (log_file != NULL)
    {
        fclose(log_file);
        log_file = NULL;
    }
}

// sd_card.cpp
bool sd_card::open_user_data_file(const char *user_data_filename)
{
    user_data_file = fopen(user_data_filename, "a");
    if (user_data_file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open user data file");
        return false;
    }
    return true;
}

void sd_card::write_user_data(const char *data)
{
    if (user_data_file != NULL)
    {
        fprintf(user_data_file, "%s\n", data);
        fflush(user_data_file); // Ensure data is written to the file
    }
}

void sd_card::close_user_data_file()
{
    if (user_data_file != NULL)
    {
        fclose(user_data_file);
        user_data_file = NULL;
    }
}

void sd_card::read_log_to_serial(const char *log_filename)
{
    FILE *file = fopen(log_filename, "r");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open log file for reading");
        return;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        printf("%s", buffer); // Output to serial
    }

    fclose(file);
}

void sd_card::read_user_data_to_serial(const char *user_data_filename)
{
    FILE *file = fopen(user_data_filename, "r");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open user data file for reading");
        return;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        printf("%s", buffer); // Output to serial
    }

    fclose(file);
}

bool sd_card::write_json_to_file(const char *filename, cJSON *json)
{
    if (!json)
    {
        ESP_LOGE(TAG, "Invalid JSON object");
        return false;
    }

    // 将 JSON 对象转换为字符串
    char *json_string = cJSON_Print(json);
    if (!json_string)
    {
        ESP_LOGE(TAG, "Failed to convert JSON to string");
        return false;
    }

    // 使用 .txt 写入
    char json_filepath[128];
    snprintf(json_filepath, sizeof(json_filepath), "%s/%s", mount_point, filename);
    ESP_LOGI(TAG, "Writing JSON to: [%s]", json_filepath);

    FILE *file = fopen(json_filepath, "w");
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to open file: %s, errno: %d", json_filepath, errno);
        cJSON_free(json_string);
        return false;
    }

    fprintf(file, "%s", json_string); // 写入 JSON 字符串
    fclose(file);
    cJSON_free(json_string);

    return true;
}

// 从文件读取 JSON 数据
cJSON *sd_card::read_json_from_file(const char *filename)
{
    // 读取 JSON 文件路径
    char json_filepath[128];
    snprintf(json_filepath, sizeof(json_filepath), "%s/%s", mount_point, filename);
    ESP_LOGI(TAG, "Reading JSON from: [%s]", json_filepath);

    // 打开文件
    FILE *file = fopen(json_filepath, "r");
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to open file: %s, errno: %d", json_filepath, errno);
        return NULL;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 分配内存并读取文件内容
    char *json_data = (char *)malloc(file_size + 1);
    if (!json_data)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON data");
        fclose(file);
        return NULL;
    }

    fread(json_data, 1, file_size, file);
    json_data[file_size] = '\0'; // 添加字符串结束符
    fclose(file);

    ESP_LOGI(TAG, "Raw JSON read from file: %s", json_data);

    // 解析 JSON 数据
    cJSON *json = cJSON_Parse(json_data);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON data from file: %s", json_filepath);
    }
    else
    {
        ESP_LOGI(TAG, "JSON successfully parsed from file: %s", json_filepath);
    }

    free(json_data); // 释放文件内容
    return json;     // 返回解析后的 JSON 对象
}