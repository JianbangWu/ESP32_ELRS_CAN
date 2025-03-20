#pragma once

#include <string>
#include <string_view>
#include <filesystem>

#ifdef __cplusplus
extern "C"
{
#endif
#include "esp_vfs_fat.h"
#include "esp_console.h"
#include "esp_log.h"

    class USER_CONSOLE
    {
    private:
        const char *TAG = "USER_CONSOLE";

        SemaphoreHandle_t &_prompt_change_sem;

        const std::string nvs_mtp = "/data";

        const std::string cmd_history_path = "/data/history.txt";

        const uint8_t CONSOLE_PROMPT_MAX_LEN = 64;
        char *prompt;
        std::string &_current_dir; // 当前工作目录
        const std::string device_name{CONFIG_IDF_TARGET};
        std::string &_wifi_state;

        void initialize_nvs();
        void initialize_filesystem();
        void initialize_console_peripheral();
        void initialize_console_library(std::string_view history_path);

        void task();
        void update_prompt(void);

    public:
        USER_CONSOLE(SemaphoreHandle_t &path_change, std::string &current_path, std::string &wifi_state);
        ~USER_CONSOLE();

        void set_device_state(std::string_view state);
        void set_path(std::string_view path);
    };

#ifdef __cplusplus
}
#endif