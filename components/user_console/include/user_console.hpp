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
        const std::string nvs_mtp = "/data";
        const std::string cmd_history_path = "/data/history.txt";
        const uint8_t CONSOLE_PROMPT_MAX_LEN = 64;
        char *prompt;
        std::string current_dir; // 当前工作目录
        const std::string device_name{CONFIG_IDF_TARGET};
        std::string device_state{""};

        const esp_vfs_fat_mount_config_t mount_config = {
            .format_if_mount_failed = true,
            .max_files = 4,
            .allocation_unit_size = 512,
            .disk_status_check_enable = true,
            .use_one_fat = false,
        };

        const esp_console_config_t console_config = {
            .max_cmdline_length = 256,
            .max_cmdline_args = 8,
            .heap_alloc_caps = MALLOC_CAP_DEFAULT,
            .hint_color = atoi(LOG_ANSI_COLOR_CYAN),
            .hint_bold = false,
        };

        wl_handle_t wl_handle;

        void initialize_nvs();
        void initialize_filesystem();
        void initialize_console_peripheral();
        void initialize_console_library(std::string_view history_path);
        void task();
        void update_prompt(void);

    public:
        USER_CONSOLE();
        ~USER_CONSOLE();

        void set_device_state(std::string_view state);
        void set_path(std::string_view path);
    };

    void register_commands(void);

#ifdef __cplusplus
}
#endif