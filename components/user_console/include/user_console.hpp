#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "esp_vfs_fat.h"
#include "esp_console.h"
#include "esp_log.h"

#define PROMPT_STR CONFIG_IDF_TARGET
    class USER_CONSOLE
    {
    private:
        const char *TAG = "USER_CONSOLE";
        const char *nvs_mtp = "/data";
        const char *cmd_history_path = "/data/history.txt";
        const uint8_t CONSOLE_PROMPT_MAX_LEN = 32;
        char *prompt;

        const esp_vfs_fat_mount_config_t mount_config = {
            .format_if_mount_failed = 1,
            .max_files = 4,
            .allocation_unit_size = 512,
            .disk_status_check_enable = 1,
            .use_one_fat = false,
        };

        /* Initialize the console */
        const esp_console_config_t console_config = {
            .max_cmdline_length = 256,
            .max_cmdline_args = 8,
            .heap_alloc_caps = MALLOC_CAP_DEFAULT,
            .hint_color = atoi(LOG_ANSI_COLOR_CYAN),
            .hint_bold = 0,
        };

        wl_handle_t wl_handle;

        void initialize_nvs(void);
        void initialize_filesystem(void);
        void initialize_console_peripheral(void);
        void initialize_console_library(const char *history_path);
        char *setup_prompt(const char *prompt_str);
        void task(void);

    public:
        USER_CONSOLE();
        ~USER_CONSOLE();
    };

#ifdef __cplusplus
}
#endif
