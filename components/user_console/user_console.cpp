#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

#include "user_console.hpp"
#include "linenoise/linenoise.h"

#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"

#include "cmd_system.h"
#include "cmd_nvs.h"

void USER_CONSOLE::task(void)
{
    while (1)
    {
        /* Get a line using linenoise.
         * The line is returned when ENTER is pressed.
         */
        char *line = linenoise(prompt);

        if (line == NULL)
        { /* Ignore empty lines */
            continue;
        }

        /* Add the command to the history if not empty*/
        if (strlen(line) > 0)
        {
            linenoiseHistoryAdd(line);

            /* Save command history to filesystem */
            linenoiseHistorySave(cmd_history_path);
        }

        /* Try to run the command */
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND)
        {
            printf("Unrecognized command\n");
        }
        else if (err == ESP_ERR_INVALID_ARG)
        {
            // command was empty
        }
        else if (err == ESP_OK && ret != ESP_OK)
        {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        }
        else if (err != ESP_OK)
        {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        /* linenoise allocates line buffer on the heap, so need to free it */
        linenoiseFree(line);
    }

    ESP_LOGE(TAG, "Error or end-of-input, terminating console");
    esp_console_deinit();
}
void USER_CONSOLE::initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize NVS after erase: %s", esp_err_to_name(err));
            return;
        }
    }
    ESP_ERROR_CHECK(err);
}

void USER_CONSOLE::initialize_filesystem(void)
{

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(nvs_mtp, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}

void USER_CONSOLE::initialize_console_peripheral(void)
{
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Enable blocking mode on stdin and stdout */
    fcntl(fileno(stdout), F_SETFL, 0);
    fcntl(fileno(stdin), F_SETFL, 0);

    usb_serial_jtag_driver_config_t jtag_config = {
        .tx_buffer_size = 256,
        .rx_buffer_size = 256,
    };

    /* Install USB-SERIAL-JTAG driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&jtag_config));

    /* Tell vfs to use usb-serial-jtag driver */
    usb_serial_jtag_vfs_use_driver();

    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);
}

void USER_CONSOLE::initialize_console_library(const char *history_path)
{
    ESP_ERROR_CHECK(esp_console_init(&console_config));
    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within
     * single line.
     */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(100);

    /* Set command maximum length */
    linenoiseSetMaxLineLen(console_config.max_cmdline_length);

    /* Don't return empty lines */
    linenoiseAllowEmpty(false);

    /* Load command history from filesystem */
    linenoiseHistoryLoad(history_path);

    /* Figure out if the terminal supports escape sequences */
    const int probe_status = linenoiseProbe();
    if (probe_status)
    { /* zero indicates success */
        linenoiseSetDumbMode(1);
    }
}

char *USER_CONSOLE::setup_prompt(const char *prompt_str)
{
    /* set command line prompt */
    const char *prompt_temp = "esp>";
    if (prompt_str)
    {
        prompt_temp = prompt_str;
    }
    snprintf(prompt, CONSOLE_PROMPT_MAX_LEN - 1, LOG_COLOR_I "%s " LOG_RESET_COLOR, prompt_temp);

    if (linenoiseIsDumbMode())
    {
        /* Since the terminal doesn't support escape sequences,
         * don't use color codes in the s_prompt.
         */
        snprintf(prompt, CONSOLE_PROMPT_MAX_LEN - 1, "%s ", prompt_temp);
    }
    return prompt;
}

USER_CONSOLE::USER_CONSOLE()
{
    initialize_nvs();
    initialize_filesystem();
    ESP_LOGI(TAG, "Command history enabled");
    // TODO
    /* Initialize console output periheral (UART, USB_OTG, USB_JTAG) */
    initialize_console_peripheral();
    /* Initialize linenoise library and esp_console*/
    initialize_console_library(cmd_history_path);
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    prompt = (char *)malloc(CONSOLE_PROMPT_MAX_LEN);
    if (prompt == nullptr)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for prompt");
        return;
    }
    setup_prompt(PROMPT_STR ">");

    /* Register commands */
    esp_console_register_help_command();
    register_system_common();
    register_nvs();

    printf("\n"
           "This is an example of ESP-IDF console component.\n"
           "Type 'help' to get the list of commands.\n"
           "Use UP/DOWN arrows to navigate through command history.\n"
           "Press TAB when typing command name to auto-complete.\n"
           "Ctrl+C will terminate the console environment.\n");

    if (linenoiseIsDumbMode())
    {
        printf("\n"
               "Your terminal application does not support escape sequences.\n"
               "Line editing and history features are disabled.\n"
               "On Windows, try using Putty instead.\n");
    }

    auto task_func = [](void *arg)
    {
        USER_CONSOLE *instance = static_cast<USER_CONSOLE *>(arg);
        instance->task(); // 调用类的成员函数
    };

    xTaskCreatePinnedToCore(task_func, "console", 1024 * 32, this, configMAX_PRIORITIES - 1, NULL, 1);
}

USER_CONSOLE::~USER_CONSOLE()
{
    if (prompt)
    {
        free(prompt);
    }
    esp_console_deinit();
}