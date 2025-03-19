#include <cstring>  // 用于 strlen 和 memcmp
#include <unistd.h> // 用于 fsync
#include "system_cmd.hpp"

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "driver/uart.h"

static const char *TAG = "CmdSystem";

void CmdSystem::registerSystem()
{
    registerSystemCommon();

#if SOC_LIGHT_SLEEP_SUPPORTED
    registerSystemLightSleep();
#endif

#if SOC_DEEP_SLEEP_SUPPORTED
    registerSystemDeepSleep();
#endif
}

void CmdSystem::registerSystemCommon()
{
    registerVersion();
    registerRestart();
    registerFree();
    registerHeap();
#if WITH_TASKS_INFO
    registerTasks();
#endif
    registerLogLevel();
}

void CmdSystem::registerSystemDeepSleep()
{
    static struct
    {
        struct arg_int *wakeup_time;
#if SOC_PM_SUPPORT_EXT0_WAKEUP || SOC_PM_SUPPORT_EXT1_WAKEUP
        struct arg_int *wakeup_gpio_num;
        struct arg_int *wakeup_gpio_level;
#endif
        struct arg_end *end;
    } deep_sleep_args;

    int num_args = 1;
    deep_sleep_args.wakeup_time = arg_int0("t", "time", "<t>", "Wake up time, ms");
#if SOC_PM_SUPPORT_EXT0_WAKEUP || SOC_PM_SUPPORT_EXT1_WAKEUP
    deep_sleep_args.wakeup_gpio_num = arg_int0(NULL, "io", "<n>", "If specified, wakeup using GPIO with given number");
    deep_sleep_args.wakeup_gpio_level = arg_int0(NULL, "io_level", "<0|1>", "GPIO level to trigger wakeup");
    num_args += 2;
#endif
    deep_sleep_args.end = arg_end(num_args);

    const esp_console_cmd_t cmd = {
        .command = "deep_sleep",
        .help = "Enter deep sleep mode. "
#if SOC_PM_SUPPORT_EXT0_WAKEUP || SOC_PM_SUPPORT_EXT1_WAKEUP
                "Two wakeup modes are supported: timer and GPIO. "
#else
                "Timer wakeup mode is supported. "
#endif
                "If no wakeup option is specified, will sleep indefinitely.",
        .hint = NULL,
        .func = &deepSleep,
        .argtable = &deep_sleep_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

void CmdSystem::registerSystemLightSleep()
{
    static struct
    {
        struct arg_int *wakeup_time;
        struct arg_int *wakeup_gpio_num;
        struct arg_int *wakeup_gpio_level;
        struct arg_end *end;
    } light_sleep_args;

    light_sleep_args.wakeup_time = arg_int0("t", "time", "<t>", "Wake up time, ms");
    light_sleep_args.wakeup_gpio_num = arg_intn(NULL, "io", "<n>", 0, 8, "If specified, wakeup using GPIO with given number");
    light_sleep_args.wakeup_gpio_level = arg_intn(NULL, "io_level", "<0|1>", 0, 8, "GPIO level to trigger wakeup");
    light_sleep_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "light_sleep",
        .help = "Enter light sleep mode. "
                "Two wakeup modes are supported: timer and GPIO. "
                "Multiple GPIO pins can be specified using pairs of "
                "'io' and 'io_level' arguments. "
                "Will also wake up on UART input.",
        .hint = NULL,
        .func = &lightSleep,
        .argtable = &light_sleep_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int CmdSystem::getVersion(int argc, char **argv)
{
    const char *model;
    esp_chip_info_t info;
    uint32_t flash_size;
    esp_chip_info(&info);

    switch (info.model)
    {
    case CHIP_ESP32:
        model = "ESP32";
        break;
    case CHIP_ESP32S2:
        model = "ESP32-S2";
        break;
    case CHIP_ESP32S3:
        model = "ESP32-S3";
        break;
    case CHIP_ESP32C3:
        model = "ESP32-C3";
        break;
    case CHIP_ESP32H2:
        model = "ESP32-H2";
        break;
    case CHIP_ESP32C2:
        model = "ESP32-C2";
        break;
    case CHIP_ESP32P4:
        model = "ESP32-P4";
        break;
    case CHIP_ESP32C5:
        model = "ESP32-C5";
        break;
    default:
        model = "Unknown";
        break;
    }

    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK)
    {
        printf("Get flash size failed");
        return 1;
    }
    printf("IDF Version:%s\r\n", esp_get_idf_version());
    printf("Chip info:\r\n");
    printf("\tmodel:%s\r\n", model);
    printf("\tcores:%d\r\n", info.cores);
    printf("\tfeature:%s%s%s%s%" PRIu32 "%s\r\n",
           info.features & CHIP_FEATURE_WIFI_BGN ? "/802.11bgn" : "",
           info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
           info.features & CHIP_FEATURE_BT ? "/BT" : "",
           info.features & CHIP_FEATURE_EMB_FLASH ? "/Embedded-Flash:" : "/External-Flash:",
           flash_size / (1024 * 1024), " MB");
    printf("\trevision number:%d\r\n", info.revision);
    return 0;
}

void CmdSystem::registerVersion()
{
    const esp_console_cmd_t cmd = {
        .command = "version",
        .help = "Get version of chip and SDK",
        .hint = NULL,
        .func = &getVersion,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int CmdSystem::restart(int argc, char **argv)
{
    ESP_LOGI(TAG, "Restarting");
    esp_restart();
}

void CmdSystem::registerRestart()
{
    const esp_console_cmd_t cmd = {
        .command = "restart",
        .help = "Software reset of the chip",
        .hint = NULL,
        .func = &restart,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int CmdSystem::freeMem(int argc, char **argv)
{
    printf("%" PRIu32 " in Kb \n", esp_get_free_heap_size() / 1024);
    return 0;
}

void CmdSystem::registerFree()
{
    const esp_console_cmd_t cmd = {
        .command = "free",
        .help = "Get the current size of free heap memory",
        .hint = NULL,
        .func = &freeMem,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

int CmdSystem::heapSize(int argc, char **argv)
{
    uint32_t heap_size = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    printf("min heap size: %" PRIu32 "\n", heap_size);
    return 0;
}

void CmdSystem::registerHeap()
{
    const esp_console_cmd_t heap_cmd = {
        .command = "heap",
        .help = "Get minimum size of free heap memory that was available during program execution",
        .hint = NULL,
        .func = &heapSize,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&heap_cmd));
}

#if WITH_TASKS_INFO
int CmdSystem::tasksInfo(int argc, char **argv)
{
    const size_t bytes_per_task = 40; /* see vTaskList description */
    char *task_list_buffer = malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == NULL)
    {
        ESP_LOGE(TAG, "failed to allocate buffer for vTaskList output");
        return 1;
    }
    fputs("Task Name\tStatus\tPrio\tHWM\tTask#", stdout);
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
    fputs("\tAffinity", stdout);
#endif
    fputs("\n", stdout);
    vTaskList(task_list_buffer);
    fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    return 0;
}

void CmdSystem::registerTasks()
{
    const esp_console_cmd_t cmd = {
        .command = "tasks",
        .help = "Get information about running tasks",
        .hint = NULL,
        .func = &tasksInfo,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
#endif // WITH_TASKS_INFO

int CmdSystem::logLevel(int argc, char **argv)
{
    static struct
    {
        struct arg_str *tag;
        struct arg_str *level;
        struct arg_end *end;
    } log_level_args;

    static const char *s_log_level_names[] = {
        "none", "error", "warn", "info", "debug", "verbose"};

    int nerrors = arg_parse(argc, argv, (void **)&log_level_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, log_level_args.end, argv[0]);
        return 1;
    }
    assert(log_level_args.tag->count == 1);
    assert(log_level_args.level->count == 1);
    const char *tag = log_level_args.tag->sval[0];
    const char *level_str = log_level_args.level->sval[0];
    esp_log_level_t level;
    size_t level_len = strlen(level_str);
    // for (level = ESP_LOG_NONE; level <= ESP_LOG_VERBOSE; level++)
    for (size_t i = 0; i < ESP_LOG_VERBOSE; i++)
    {
        level = static_cast<esp_log_level_t>(i);
        if (memcmp(level_str, s_log_level_names[level], level_len) == 0)
        {
            break;
        }
    }
    if (level > ESP_LOG_VERBOSE)
    {
        printf("Invalid log level '%s', choose from none|error|warn|info|debug|verbose\n", level_str);
        return 1;
    }
    if (level > CONFIG_LOG_MAXIMUM_LEVEL)
    {
        printf("Can't set log level to %s, max level limited in menuconfig to %s. "
               "Please increase CONFIG_LOG_MAXIMUM_LEVEL in menuconfig.\n",
               s_log_level_names[level], s_log_level_names[CONFIG_LOG_MAXIMUM_LEVEL]);
        return 1;
    }
    esp_log_level_set(tag, level);
    return 0;
}

void CmdSystem::registerLogLevel()
{
    static struct
    {
        struct arg_str *tag;
        struct arg_str *level;
        struct arg_end *end;
    } log_level_args;

    log_level_args.tag = arg_str1(NULL, NULL, "<tag|*>", "Log tag to set the level for, or * to set for all tags");
    log_level_args.level = arg_str1(NULL, NULL, "<none|error|warn|debug|verbose>", "Log level to set. Abbreviated words are accepted.");
    log_level_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "log_level",
        .help = "Set log level for all tags or a specific tag.",
        .hint = NULL,
        .func = &logLevel,
        .argtable = &log_level_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

#if SOC_DEEP_SLEEP_SUPPORTED
int CmdSystem::deepSleep(int argc, char **argv)
{
    static struct
    {
        struct arg_int *wakeup_time;
#if SOC_PM_SUPPORT_EXT0_WAKEUP || SOC_PM_SUPPORT_EXT1_WAKEUP
        struct arg_int *wakeup_gpio_num;
        struct arg_int *wakeup_gpio_level;
#endif
        struct arg_end *end;
    } deep_sleep_args;

    int nerrors = arg_parse(argc, argv, (void **)&deep_sleep_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, deep_sleep_args.end, argv[0]);
        return 1;
    }
    if (deep_sleep_args.wakeup_time->count)
    {
        uint64_t timeout = 1000ULL * deep_sleep_args.wakeup_time->ival[0];
        ESP_LOGI(TAG, "Enabling timer wakeup, timeout=%lluus", timeout);
        ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(timeout));
    }

#if SOC_PM_SUPPORT_EXT1_WAKEUP
    if (deep_sleep_args.wakeup_gpio_num->count)
    {
        int io_num = deep_sleep_args.wakeup_gpio_num->ival[0];
        if (!esp_sleep_is_valid_wakeup_gpio(static_cast<gpio_num_t>(io_num)))
        {
            ESP_LOGE(TAG, "GPIO %d is not an RTC IO", io_num);
            return 1;
        }
        int level = 0;
        if (deep_sleep_args.wakeup_gpio_level->count)
        {
            level = deep_sleep_args.wakeup_gpio_level->ival[0];
            if (level != 0 && level != 1)
            {
                ESP_LOGE(TAG, "Invalid wakeup level: %d", level);
                return 1;
            }
        }
        ESP_LOGI(TAG, "Enabling wakeup on GPIO%d, wakeup on %s level",
                 io_num, level ? "HIGH" : "LOW");

        ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(1ULL << io_num, static_cast<esp_sleep_ext1_wakeup_mode_t>(level)));
        ESP_LOGE(TAG, "GPIO wakeup from deep sleep currently unsupported on ESP32-C3");
    }
#endif // SOC_PM_SUPPORT_EXT1_WAKEUP

#if CONFIG_IDF_TARGET_ESP32
    rtc_gpio_isolate(GPIO_NUM_12);
#endif // CONFIG_IDF_TARGET_ESP32
    esp_deep_sleep_start();
    return 1;
}

void CmdSystem::registerDeepSleep()
{
    static struct
    {
        struct arg_int *wakeup_time;
#if SOC_PM_SUPPORT_EXT0_WAKEUP || SOC_PM_SUPPORT_EXT1_WAKEUP
        struct arg_int *wakeup_gpio_num;
        struct arg_int *wakeup_gpio_level;
#endif
        struct arg_end *end;
    } deep_sleep_args;

    int num_args = 1;
    deep_sleep_args.wakeup_time = arg_int0("t", "time", "<t>", "Wake up time, ms");
#if SOC_PM_SUPPORT_EXT0_WAKEUP || SOC_PM_SUPPORT_EXT1_WAKEUP
    deep_sleep_args.wakeup_gpio_num = arg_int0(NULL, "io", "<n>", "If specified, wakeup using GPIO with given number");
    deep_sleep_args.wakeup_gpio_level = arg_int0(NULL, "io_level", "<0|1>", "GPIO level to trigger wakeup");
    num_args += 2;
#endif
    deep_sleep_args.end = arg_end(num_args);

    const esp_console_cmd_t cmd = {
        .command = "deep_sleep",
        .help = "Enter deep sleep mode. "
#if SOC_PM_SUPPORT_EXT0_WAKEUP || SOC_PM_SUPPORT_EXT1_WAKEUP
                "Two wakeup modes are supported: timer and GPIO. "
#else
                "Timer wakeup mode is supported. "
#endif
                "If no wakeup option is specified, will sleep indefinitely.",
        .hint = NULL,
        .func = &deepSleep,
        .argtable = &deep_sleep_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
#endif // SOC_DEEP_SLEEP_SUPPORTED

#if SOC_LIGHT_SLEEP_SUPPORTED
int CmdSystem::lightSleep(int argc, char **argv)
{
    static struct
    {
        struct arg_int *wakeup_time;
        struct arg_int *wakeup_gpio_num;
        struct arg_int *wakeup_gpio_level;
        struct arg_end *end;
    } light_sleep_args;

    int nerrors = arg_parse(argc, argv, (void **)&light_sleep_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, light_sleep_args.end, argv[0]);
        return 1;
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (light_sleep_args.wakeup_time->count)
    {
        uint64_t timeout = 1000ULL * light_sleep_args.wakeup_time->ival[0];
        ESP_LOGI(TAG, "Enabling timer wakeup, timeout=%lluus", timeout);
        ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(timeout));
    }
    int io_count = light_sleep_args.wakeup_gpio_num->count;
    if (io_count != light_sleep_args.wakeup_gpio_level->count)
    {
        ESP_LOGE(TAG, "Should have same number of 'io' and 'io_level' arguments");
        return 1;
    }
    for (int i = 0; i < io_count; ++i)
    {
        int io_num = light_sleep_args.wakeup_gpio_num->ival[i];
        int level = light_sleep_args.wakeup_gpio_level->ival[i];
        if (level != 0 && level != 1)
        {
            ESP_LOGE(TAG, "Invalid wakeup level: %d", level);
            return 1;
        }
        ESP_LOGI(TAG, "Enabling wakeup on GPIO%d, wakeup on %s level",
                 io_num, level ? "HIGH" : "LOW");

        ESP_ERROR_CHECK(gpio_wakeup_enable(static_cast<gpio_num_t>(io_num), level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL));
    }
    if (io_count > 0)
    {
        ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
    }
    if (CONFIG_ESP_CONSOLE_UART_NUM >= 0 && CONFIG_ESP_CONSOLE_UART_NUM <= UART_NUM_1)
    {
        ESP_LOGI(TAG, "Enabling UART wakeup (press ENTER to exit light sleep)");
        ESP_ERROR_CHECK(uart_set_wakeup_threshold(static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM), 3));
        ESP_ERROR_CHECK(esp_sleep_enable_uart_wakeup(static_cast<uart_port_t>(CONFIG_ESP_CONSOLE_UART_NUM)));
    }
    fflush(stdout);
    fsync(fileno(stdout));
    esp_light_sleep_start();
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    const char *cause_str;
    switch (cause)
    {
    case ESP_SLEEP_WAKEUP_GPIO:
        cause_str = "GPIO";
        break;
    case ESP_SLEEP_WAKEUP_UART:
        cause_str = "UART";
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        cause_str = "timer";
        break;
    default:
        cause_str = "unknown";
        printf("%d\n", cause);
    }
    ESP_LOGI(TAG, "Woke up from: %s", cause_str);
    return 0;
}

void CmdSystem::registerLightSleep()
{
    static struct
    {
        struct arg_int *wakeup_time;
        struct arg_int *wakeup_gpio_num;
        struct arg_int *wakeup_gpio_level;
        struct arg_end *end;
    } light_sleep_args;

    light_sleep_args.wakeup_time = arg_int0("t", "time", "<t>", "Wake up time, ms");
    light_sleep_args.wakeup_gpio_num = arg_intn(NULL, "io", "<n>", 0, 8, "If specified, wakeup using GPIO with given number");
    light_sleep_args.wakeup_gpio_level = arg_intn(NULL, "io_level", "<0|1>", 0, 8, "GPIO level to trigger wakeup");
    light_sleep_args.end = arg_end(3);

    const esp_console_cmd_t cmd = {
        .command = "light_sleep",
        .help = "Enter light sleep mode. "
                "Two wakeup modes are supported: timer and GPIO. "
                "Multiple GPIO pins can be specified using pairs of "
                "'io' and 'io_level' arguments. "
                "Will also wake up on UART input.",
        .hint = NULL,
        .func = &lightSleep,
        .argtable = &light_sleep_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
#endif // SOC_LIGHT_SLEEP_SUPPORTED