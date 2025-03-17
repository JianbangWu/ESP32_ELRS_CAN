#include "ntp_service.hpp"

#include <ctime>
#include <sys/time.h>

#include "esp_console.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include <argtable3/argtable3.h>

// 定义静态成员变量
SNTPManager::NtpArgs SNTPManager::ntp_args;

SNTPManager::SNTPManager()
{
    // 初始化 TCP/IP 和事件循环
    ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建事件组
    event_group = xEventGroupCreate();
    if (event_group == NULL)
    {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // 注册命令行命令
    register_ntp_command();
}

SNTPManager::~SNTPManager()
{
    // 删除事件组
    if (event_group != NULL)
    {
        vEventGroupDelete(event_group);
    }
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
        .func = &start_ntp_command_handler,
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

    const char *ntp_server = ntp_args.server->sval[0];

    ESP_LOGI(TAG, "Starting NTP service with server: %s", ntp_server);

    // 初始化并启动 SNTP 服务
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(ntp_server);
    config.sync_cb = [](struct timeval *tv)
    {
        ESP_LOGI(TAG, "Time synchronized");
    };
    esp_netif_sntp_init(&config);
    esp_netif_sntp_start();

    setenv("TZ", "CST-8", 1);

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
        size_t bufferSize = 64;
        char buffer[bufferSize];
        std::strftime(buffer, bufferSize, "%Y-%m-%d-%H-%M-%S \r\n", &timeinfo);
        printf("%s", buffer);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to synchronize time");
    }

    esp_netif_sntp_deinit();

    return 0;
}
