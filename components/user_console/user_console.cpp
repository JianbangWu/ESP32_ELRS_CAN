#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>

#include "user_console.hpp"
#include "linenoise/linenoise.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_cdcacm.h"
#include "esp_vfs_console.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"

namespace fs = std::filesystem;
static const uint32_t StackSize = 1024 * 16;

void USER_CONSOLE::task()
{
    while (true)
    {
        if (xSemaphoreTake(_prompt_change_sem, 0))
        {
            update_prompt();
        }

        char *line = linenoise(prompt);
        if (!line)
            continue; // 忽略空行

        if (strlen(line) > 0)
        {
            linenoiseHistoryAdd(line);
            linenoiseHistorySave(cmd_history_path.c_str());
        }

        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        if (err == ESP_ERR_NOT_FOUND)
        {
            printf("Unrecognized command\n");
        }
        else if (err == ESP_ERR_INVALID_ARG)
        {
            // 命令为空
        }
        else if (err == ESP_OK && ret != ESP_OK)
        {
            printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        }
        else if (err != ESP_OK)
        {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }

        linenoiseFree(line);
    }

    ESP_LOGE(TAG, "Error or end-of-input, terminating console");
    esp_console_deinit();
}

void USER_CONSOLE::initialize_nvs()
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

void USER_CONSOLE::initialize_filesystem()
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = 512,
        .disk_status_check_enable = true,
        .use_one_fat = false,
    };

    wl_handle_t wl_handle;
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(nvs_mtp.c_str(), "storage", &mount_config, &wl_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}

void USER_CONSOLE::initialize_console_peripheral()
{
    fflush(stdout);
    fsync(fileno(stdout));

#if defined(CONFIG_ESP_CONSOLE_USB_CDC)
    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_cdcacm_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_cdcacm_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Enable blocking mode on stdin and stdout */
    fcntl(fileno(stdout), F_SETFL, 0);
    fcntl(fileno(stdin), F_SETFL, 0);

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)

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
#elif defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    uart_vfs_dev_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

    /* Configure UART. Note that REF_TICK is used so that the baud rate remains
     * correct while APB frequency is changing in light sleep mode.
     */
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
#if SOC_UART_SUPPORT_REF_TICK
        .source_clk = UART_SCLK_REF_TICK,
#elif SOC_UART_SUPPORT_XTAL_CLK
        .source_clk = UART_SCLK_XTAL,
#endif
    };
    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK(uart_driver_install(static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM), 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM), &uart_config));

    /* Tell VFS to use UART driver */
    uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

#else
#error Unsupported console type
#endif
    setvbuf(stdin, nullptr, _IONBF, 0);
}

void USER_CONSOLE::initialize_console_library(std::string_view history_path)
{

    const esp_console_config_t console_config = {
        .max_cmdline_length = 256,
        .max_cmdline_args = 8,
        .heap_alloc_caps = MALLOC_CAP_DEFAULT,
        .hint_color = atoi(LOG_ANSI_COLOR_CYAN),
        .hint_bold = false,
    };

    ESP_ERROR_CHECK(esp_console_init(&console_config));

    linenoiseSetMultiLine(true);
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);
    linenoiseHistorySetMaxLen(100);
    linenoiseSetMaxLineLen(console_config.max_cmdline_length);
    linenoiseAllowEmpty(false);
    linenoiseHistoryLoad(history_path.data());

    if (linenoiseProbe())
    {
        linenoiseSetDumbMode(true);
    }
}

void USER_CONSOLE::update_prompt(void)
{
    snprintf(prompt, CONSOLE_PROMPT_MAX_LEN - 1,
             LOG_COLOR_I "%s " LOG_COLOR_V "%s" LOG_RESET_COLOR ":%s>", device_name.c_str(), _wifi_state.c_str(), _current_dir.c_str());
}

USER_CONSOLE::USER_CONSOLE(SemaphoreHandle_t &prompt_change,
                           std::string &current_path,
                           std::string &wifi_state) : _prompt_change_sem(prompt_change),
                                                      _current_dir(current_path),
                                                      _wifi_state(wifi_state)

{
    if (prompt_change == nullptr)
    {
        ESP_LOGE(TAG, "Invalid queue handle provided");
        return; // 或者抛出异常
    }

    // initialize_nvs();
    // initialize_filesystem();
    ESP_LOGI(TAG, "Command history enabled");

    initialize_console_peripheral();
    initialize_console_library(cmd_history_path);

    prompt = (char *)malloc(CONSOLE_PROMPT_MAX_LEN);
    if (!prompt)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for prompt");
        return;
    }

    esp_console_register_help_command();

    update_prompt();

    printf("Type 'help' to get the list of commands.\n"
           "Use UP/DOWN arrows to navigate through command history.\n"
           "Press TAB when typing command name to auto-complete.\n"
           "Ctrl+C will terminate the console environment.\n");

    if (linenoiseIsDumbMode())
    {
        printf("\nYour terminal application does not support escape sequences.\n"
               "Line editing and history features are disabled.\n"
               "On Windows, try using Putty instead.\n");
    }

    auto task_func = [](void *arg)
    {
        USER_CONSOLE *instance = static_cast<USER_CONSOLE *>(arg);
        instance->task();
    };

    xTaskCreatePinnedToCore(task_func, "console", StackSize, this, configMAX_PRIORITIES - 1, nullptr, 1);
}

USER_CONSOLE::~USER_CONSOLE()
{
    if (prompt)
    {
        free(prompt);
    }
    esp_console_deinit();
}
