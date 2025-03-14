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

    class Buzzer
    {
    public:
        Buzzer(const gpio_num_t gpio_num = GPIO_NUM_10,
               ledc_clk_cfg_t clk_cfg = LEDC_AUTO_CLK,
               ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE,
               ledc_timer_bit_t timer_bit = LEDC_TIMER_13_BIT,
               ledc_timer_t timer_num = LEDC_TIMER_0,
               ledc_channel_t channel = LEDC_CHANNEL_0,
               uint32_t idle_level = 0);
        virtual ~Buzzer();
        BaseType_t Beep(uint32_t frequency, uint32_t duration_ms);

    private:
        struct message_t
        {
            uint32_t frequency;
            uint32_t duration_ms;
        };

        void Stop();
        void Start(uint32_t frequency);
        void Task();
        static void TaskForwarder(void *param)
        {
            Buzzer *buzzer = (Buzzer *)param;
            buzzer->Task();
        }

        TaskHandle_t task_handle_;
        QueueHandle_t queue_handle_;
        ledc_mode_t speed_mode_;
        ledc_timer_bit_t timer_bit_;
        ledc_timer_t timer_num_;
        ledc_channel_t channel_;
        uint32_t idle_level_;
    };

#ifdef __cplusplus
}
#endif
