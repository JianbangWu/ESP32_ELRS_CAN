#include <sys/stat.h>
#include <stdint.h>
#include <sys/unistd.h>
#include <errno.h>

#include "sd_card.hpp"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

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
        if (xSemaphoreTake(det_sem, pdMS_TO_TICKS(100)))
        {
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

    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    slot_config.width = 4;

    slot_config.clk = clk_pin;
    slot_config.cmd = cmd_pin;
    slot_config.d0 = d0_pin;

    slot_config.d1 = d1_pin;
    slot_config.d2 = d2_pin;
    slot_config.d3 = d3_pin;

    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

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

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mt.c_str(), &host, &slot_config, &mount_config, &card);

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

    ESP_LOGI(TAG, "SD Card mounted at: %s", mt.c_str());

    sdmmc_card_print_info(stdout, card);

    struct stat st;
    if (stat("/sdcard", &st) != 0)
    {
        ESP_LOGE(TAG, "SD Card mount point does not exist!");
    }
}

void SDCard::unmount_sd(void)
{
    esp_vfs_fat_sdcard_unmount(mt.c_str(), card);
    ESP_LOGI(TAG, "Card unmounted");
}

void SDCard::format_sd(void)
{
    esp_err_t err = esp_vfs_fat_sdcard_format_cfg(mt.c_str(), card, &mount_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Card Format Err := %x", err);
    }
}

bool SDCard::isFileSizeExceeded(std::string filename)
{
    FILE *file = fopen(filename.c_str(), "rb"); // 以二进制只读模式打开文件
    if (!file)
    {
        return false;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);    // 移动到文件末尾
    long fileSize = ftell(file); // 获取文件大小
    fclose(file);                // 关闭文件

    // 检查文件大小是否超过限制
    return fileSize > MAX_FILE_SIZE;
}

bool SDCard::writeFile(const std::string &filename, const std::vector<uint8_t> &data)
{
    FILE *file = fopen(filename.c_str(), "ab"); // 以二进制追加模式打开文件
    if (!file)
    {
        // 文件打开失败，可能是路径错误或文件系统未挂载
        return false;
    }

    // 写入数据
    size_t bytesWritten = fwrite(data.data(), sizeof(uint8_t), data.size(), file);
    fclose(file); // 关闭文件

    // 检查是否成功写入所有数据
    return bytesWritten == data.size();
}

void SDCard::read_user_data_to_serial(const char *user_data_filename)
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

std::vector<std::string> SDCard::getFileList(const std::string &directory)
{
    std::vector<std::string> fileList;

    // 打开目录
    DIR *dir = opendir(directory.c_str());
    if (!dir)
    {
        // 目录打开失败，可能是路径错误或文件系统未挂载
        perror("Failed to open directory");
        return fileList;
    }

    // 遍历目录中的每个条目
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // 忽略 "." 和 ".." 目录
        if (entry->d_name[0] == '.')
        {
            continue;
        }

        // 将文件名添加到列表中
        fileList.push_back(entry->d_name);
    }

    // 关闭目录
    closedir(dir);

    return fileList;
}

bool SDCard::write_json_to_file(const char *filename, cJSON *json)
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
    snprintf(json_filepath, sizeof(json_filepath), "%s/%s", mt.c_str(), filename);
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
cJSON *SDCard::read_json_from_file(const char *filename)
{
    // 读取 JSON 文件路径
    char json_filepath[128];
    snprintf(json_filepath, sizeof(json_filepath), "%s/%s", mt.c_str(), filename);
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