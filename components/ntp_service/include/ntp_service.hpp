#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "esp_event.h"
#include "esp_sntp.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

    class SNTPManager
    {
    public:
        SNTPManager();
        ~SNTPManager();

    private:
        static constexpr const char *TAG = "SNTPManager";
        EventGroupHandle_t event_group;
        static constexpr EventBits_t NETWORK_CONNECTED_BIT = (1 << 0); // 网络连接事件标志位

        // 命令行参数结构体
        static struct NtpArgs
        {
            struct arg_str *server;
            struct arg_end *end;
        } ntp_args;

        // 注册 NTP 命令行命令
        void register_ntp_command();

        // 命令行命令处理函数
        static int start_ntp_command_handler(int argc, char **argv);
    };

#ifdef __cplusplus
}
#endif