#include "beep.hpp"

#include <inttypes.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const uint32_t QueueSize = 10;
static const uint32_t StackSize = 1024 * 2;

Buzzer::Buzzer(const gpio_num_t gpio_num,
               ledc_clk_cfg_t clk_cfg,
               ledc_mode_t speed_mode,
               ledc_timer_bit_t timer_bit,
               ledc_timer_t timer_num,
               ledc_channel_t channel,
               uint32_t idle_level)
    : speed_mode_(speed_mode),
      timer_bit_(timer_bit),
      timer_num_(timer_num),
      channel_(channel),
      idle_level_(idle_level)
{
    // Configure Timer
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = speed_mode_;
    ledc_timer.timer_num = timer_num_;
    ledc_timer.duty_resolution = timer_bit_;
    ledc_timer.freq_hz = 4000; // Can be any value, but put something valid
    ledc_timer.clk_cfg = clk_cfg;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Configure Channel
    ledc_channel_config_t ledc_channel = {};
    ledc_channel.speed_mode = speed_mode_;
    ledc_channel.channel = channel_;
    ledc_channel.timer_sel = timer_num_;
    ledc_channel.gpio_num = gpio_num;
    ledc_channel.duty = 0;
    ledc_channel.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_ERROR_CHECK(ledc_stop(speed_mode_, channel_, idle_level_));

    queue_handle_ = xQueueCreate(QueueSize, sizeof(uint8_t));

    auto task_func = [](void *arg)
    {
        Buzzer *instance = static_cast<Buzzer *>(arg);
        instance->Task(); // 调用类的成员函数
    };

    BaseType_t res = xTaskCreate(
        task_func, "buzzer_task", StackSize, this, uxTaskPriorityGet(nullptr), &task_handle_);

    assert(res == pdPASS);
}

void Buzzer::Stop()
{
    BaseType_t err = ledc_set_duty(speed_mode_, channel_, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to set duty cycle to zero");
    }
    err = ledc_update_duty(speed_mode_, channel_);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to update duty cycle");
    }
    err = ledc_stop(speed_mode_, channel_, idle_level_);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to stop buzzer");
    }
}

void Buzzer::Start(uint32_t frequency)
{
    uint32_t duty = (1 << timer_bit_) / 2;

    esp_err_t err = ledc_set_duty(speed_mode_, channel_, duty);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to set duty cycle");
        return;
    }
    err = ledc_update_duty(speed_mode_, channel_);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to update duty cycle");
        return;
    }
    err = ledc_set_freq(speed_mode_, timer_num_, frequency);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to set frequency");
        return;
    }
}

void Buzzer::Task()
{
    struct BeeperMessage message;
    TickType_t waitTime = portMAX_DELAY;

    while (1)
    {
        BaseType_t res = xQueueReceive(queue_handle_, &message, waitTime);
        if (res != pdPASS)
        {
            ESP_LOGD(Tag, "No new message, Stop beeping");
            Stop();
            waitTime = portMAX_DELAY;
        }
        else
        {
            ESP_LOGD(Tag,
                     "Beep at %" PRIu32 " Hz for %" PRIu32 " ms",
                     message.frequency,
                     message.duration);
            Start(message.frequency);
            waitTime = pdMS_TO_TICKS(message.duration);
        }
    }
}

Buzzer::~Buzzer()
{
    vQueueDelete(queue_handle_);
    vTaskDelete(task_handle_);
}

BaseType_t Buzzer::Beep(uint32_t frequency, uint32_t duration_ms)
{
    struct BeeperMessage message = {
        .frequency = frequency,
        .duration = duration_ms,
    };
    return xQueueSend(queue_handle_, &message, 0);
}

QueueHandle_t &Buzzer::get_beep_handle(void)
{
    return queue_handle_;
}
