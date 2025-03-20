#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

#include "logger.hpp"

#ifdef __cplusplus
extern "C"
{
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "driver/twai.h"
#include "esp_err.h"
#include "esp_log.h"

    class TWAI_Device
    {
    public:
        // 构造函数:初始化TWAI设备
        TWAI_Device(QueueHandle_t &tx_queue,
                    QueueHandle_t &rx_queue,
                    std::chrono::time_point<std::chrono::steady_clock> &origin_time,
                    gpio_num_t tx_gpio_num = GPIO_NUM_5,
                    gpio_num_t rx_gpio_num = GPIO_NUM_6,
                    gpio_num_t std_gpio_num = GPIO_NUM_4,
                    twai_timing_config_t timing_config = TWAI_TIMING_CONFIG_250KBITS(),
                    twai_filter_config_t filter_config = TWAI_FILTER_CONFIG_ACCEPT_ALL());

        // 析构函数:清理资源
        ~TWAI_Device();

        void init_io();

        void bus_enbale(bool enbale);

        // 发送消息到TWAI总线
        void send_message(const twai_message_t &message);

        // 从TWAI总线接收消息
        bool receive_message(twai_message_t &message, TickType_t timeout = portMAX_DELAY);

    private:
        const char *TAG = "TWAI";

        const std::chrono::time_point<std::chrono::steady_clock> &_origin_time;

        // 初始化TWAI驱动和任务
        void
        init();

        // 清理TWAI驱动和资源
        void deinit();

        // 后台任务:处理发送消息
        static void tx_task(void *arg);

        // 后台任务:处理接收消息
        static void rx_task(void *arg);

        static void rx_log(void *arg);

        std::string format_asc(int channel,
                               uint32_t canId,
                               const std::string &direction,
                               int dataLength,
                               const uint8_t *data);

        std::string get_timestamp(std::time_t &time);

        gpio_num_t &_tx_gpio_num;             // TX引脚
        gpio_num_t &_rx_gpio_num;             // RX引脚
        gpio_num_t &_std_gpio_num;            // RX引脚
        twai_timing_config_t &_timing_config; // TWAI时序配置
        twai_filter_config_t &_filter_config; // TWAI过滤器配置
        QueueHandle_t &_tx_queue;             // 发送消息队列
        QueueHandle_t &_rx_queue;             // 接收消息队列

        LoggerBase _twai_logger;
    };

#ifdef __cplusplus
}
#endif
