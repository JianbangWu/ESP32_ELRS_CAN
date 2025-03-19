#include "beep.hpp"

#include <inttypes.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const uint32_t QueueSize = 10;
static const uint32_t StackSize = 1024 * 2;

Buzzer::Buzzer(QueueHandle_t &queue,
               const gpio_num_t gpio_num,
               ledc_clk_cfg_t clk_cfg,
               ledc_mode_t speed_mode,
               ledc_timer_bit_t timer_bit,
               ledc_timer_t timer_num,
               ledc_channel_t channel,
               uint32_t idle_level)
    : _queue_handle(queue),
      _speed_mode(speed_mode),
      _timer_bit(timer_bit),
      _timer_num(timer_num),
      _channel(channel),
      idle_level_(idle_level)
{
    // Configure Timer
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = _speed_mode;
    ledc_timer.timer_num = _timer_num;
    ledc_timer.duty_resolution = _timer_bit;
    ledc_timer.freq_hz = 4000; // Can be any value, but put something valid
    ledc_timer.clk_cfg = clk_cfg;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Configure Channel
    ledc_channel_config_t ledc_channel = {};
    ledc_channel.speed_mode = _speed_mode;
    ledc_channel.channel = _channel;
    ledc_channel.timer_sel = _timer_num;
    ledc_channel.gpio_num = gpio_num;
    ledc_channel.duty = 0;
    ledc_channel.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_ERROR_CHECK(ledc_stop(_speed_mode, _channel, idle_level_));

    auto task_func = [](void *arg)
    {
        Buzzer *instance = static_cast<Buzzer *>(arg);
        instance->Task(); // 调用类的成员函数
    };

    BaseType_t res = xTaskCreate(
        task_func, "buzzer_task", StackSize, this, uxTaskPriorityGet(nullptr), &_task_handle);

    assert(res == pdPASS);
}

void Buzzer::Stop()
{
    BaseType_t err = ledc_set_duty(_speed_mode, _channel, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to set duty cycle to zero");
    }
    err = ledc_update_duty(_speed_mode, _channel);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to update duty cycle");
    }
    err = ledc_stop(_speed_mode, _channel, idle_level_);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to stop buzzer");
    }
}

void Buzzer::Start(uint32_t frequency)
{
    uint32_t duty = (1 << _timer_bit) / 2;

    esp_err_t err = ledc_set_duty(_speed_mode, _channel, duty);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to set duty cycle");
        return;
    }
    err = ledc_update_duty(_speed_mode, _channel);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to update duty cycle");
        return;
    }
    err = ledc_set_freq(_speed_mode, _timer_num, frequency);
    if (err != ESP_OK)
    {
        ESP_LOGE(Tag, "Failed to set frequency");
        return;
    }
}

void Buzzer::Task()
{
    struct BeeperMessage message;
    TickType_t waitTime = pdMS_TO_TICKS(100);

    while (1)
    {
        BaseType_t res = xQueueReceive(_queue_handle, &message, waitTime);
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
    vQueueDelete(_queue_handle);
    vTaskDelete(_task_handle);
}

BaseType_t Buzzer::Beep(uint32_t frequency, uint32_t duration_ms)
{
    struct BeeperMessage message = {
        .frequency = frequency,
        .duration = duration_ms,
    };
    return xQueueSend(_queue_handle, &message, 0);
}

QueueHandle_t &Buzzer::get_beep_handle(void)
{
    return _queue_handle;
}
