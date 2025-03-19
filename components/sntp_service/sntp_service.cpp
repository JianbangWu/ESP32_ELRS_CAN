#include "sntp_service.hpp"

#include <ctime>
#include <time.h>
#include <argtable3/argtable3.h>

#include "esp_console.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_netif_types.h"
#include "esp_sntp.h"

static const uint32_t StackSize = 1024 * 10;

// 定义静态成员变量
SNTPManager::NtpArgs SNTPManager::ntp_args;

EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

void SNTPManager::task(void)
{
    while (1)
    {
        if (xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                                pdFALSE, pdTRUE, pdMS_TO_TICKS(1000)) &&
            now_syn_yet == 1)
        {
            now_syn_yet = 0;
            ESP_LOGI(TAG, "Got IP address");
            start_sntp_service("ntp.aliyun.com"); // 启动 SNTP 服务
            xSemaphoreGive(_sntp_sem);
        }
    }
}

SNTPManager::SNTPManager(SemaphoreHandle_t &sntp_sem) : _sntp_sem(sntp_sem)
{
    wifi_event_group = xEventGroupCreate();

    if (_sntp_sem == nullptr)
    {
        ESP_LOGE(TAG, "Invalid Semaphore provided");
        return; // 或者抛出异常
    }

    // 初始化 TCP/IP 和事件循环
    ESP_ERROR_CHECK(esp_netif_init());

    auto task_func = [](void *arg)
    {
        SNTPManager *instance = static_cast<SNTPManager *>(arg);
        instance->task(); // 调用类的成员函数
    };

    xTaskCreate(task_func, "sntp", StackSize, this, 10, NULL);

    // 注册命令行命令
    register_ntp_command();
}

SNTPManager::~SNTPManager()
{
}

// 注册 NTP 命令行命令
void SNTPManager::register_ntp_command()
{
    ntp_args.server = arg_str1(NULL, NULL, "<server>", "NTP server address");
    ntp_args.end = arg_end(1);

    const esp_console_cmd_t ntp_cmd = {
        .command = "start_ntp",
        .help = "Start NTP service with the specified server",
        .hint = NULL,
        .func = start_ntp_command_handler,
        .argtable = &ntp_args,
        .func_w_context = nullptr,
        .context = nullptr,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&ntp_cmd));
}

// 命令行命令处理函数
int SNTPManager::start_ntp_command_handler(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&ntp_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, ntp_args.end, argv[0]);
        return 1;
    }

    const char *ntp_server = (ntp_args.server->count == 0) ? ("ntp.aliyun.com") : (ntp_args.server->sval[0]);

    ESP_LOGI(TAG, "Starting NTP service with server: %s", ntp_server);

    start_sntp_service(ntp_server);

    return 0;
}

// 启动 SNTP 服务
void SNTPManager::start_sntp_service(const char *ntp_server)
{
    ESP_LOGI(TAG, "Starting NTP service with server: %s", ntp_server);

    // 初始化并启动 SNTP 服务
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(ntp_server);
    config.smooth_sync = true;

    config.sync_cb = [](struct timeval *tv)
    {
        ESP_LOGI(TAG, "Time synchronized");
    };

    esp_netif_sntp_init(&config);
    esp_netif_sntp_start();

    setenv("TZ", "CST-8", 1);
    tzset();

    // 等待时间同步完成
    time_t now = 0;
    struct tm timeinfo{0};
    int retry = 0;
    const int retry_count = 15;

    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }

    if (retry < retry_count)
    {
        time(&now);
        localtime_r(&now, &timeinfo);
        ESP_LOGI(TAG, "Time synchronized successfully");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to synchronize time");
    }

    time(&now);
    localtime_r(&now, &timeinfo);

    const size_t bufferSize = 64;
    char buffer[bufferSize];

    size_t resultSize = std::strftime(buffer, bufferSize, "%Y-%m-%d-%H-%M-%S", &timeinfo);

    ESP_LOGI("SYS_TIME", "%s", buffer);

    esp_netif_sntp_deinit();
}
