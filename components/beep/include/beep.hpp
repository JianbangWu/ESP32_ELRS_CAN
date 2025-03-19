#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

    // 定义蜂鸣器消息类型
    struct BeeperMessage
    {
        uint32_t frequency; // 频率
        uint32_t duration;  // 持续时间（周期）
    };

    class Buzzer
    {
    public:
        Buzzer(QueueHandle_t &queue,
               const gpio_num_t gpio_num = GPIO_NUM_10,
               ledc_clk_cfg_t clk_cfg = LEDC_AUTO_CLK,
               ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE,
               ledc_timer_bit_t timer_bit = LEDC_TIMER_13_BIT,
               ledc_timer_t timer_num = LEDC_TIMER_0,
               ledc_channel_t channel = LEDC_CHANNEL_0,
               uint32_t idle_level = 0);
        virtual ~Buzzer();
        BaseType_t Beep(uint32_t frequency, uint32_t duration_ms);
        QueueHandle_t &get_beep_handle(void);

    private:
        const char *Tag = "Buzzer";

        void Stop();
        void Start(uint32_t frequency);
        void Task();

        TaskHandle_t _task_handle;
        QueueHandle_t _queue_handle;
        ledc_mode_t _speed_mode;
        ledc_timer_bit_t _timer_bit;
        ledc_timer_t _timer_num;
        ledc_channel_t _channel;
        uint32_t idle_level_;
    };

#ifdef __cplusplus
}
#endif
