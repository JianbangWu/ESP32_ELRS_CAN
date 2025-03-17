#include <stdio.h>
#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "ds3231m.hpp"

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void RTC::gpio_task(void)
{
    gpio_num_t io_num;
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            ESP_LOGI(TAG, "INT");
        }
    }
}

RTC::RTC()
{
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));
    ESP_ERROR_CHECK(i2c_master_bus_add_device((bus_handle), &(dev_cfg), &(dev_handle)));
    init_io();
}

RTC::~RTC()
{
    ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
    printf("I2C Bus Is Free! \r\n");
}

void RTC::init_io(void)
{
    /* IO */
    gpio_config_t io_conf = {};
    /* RST_PIN */
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << rst_io_pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    /* INT PIN */
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << int_io_pin);
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    auto task_func = [](void *arg)
    {
        RTC *instance = static_cast<RTC *>(arg);
        instance->gpio_task(); // 调用类的成员函数
    };

    xTaskCreate(task_func, "rtc_int", 2048, NULL, 10, NULL);

    esp_err_t ret = gpio_install_isr_service(0);

    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "ISR install fail");
        return;
    }
    gpio_isr_handler_add(int_io_pin, gpio_isr_handler, (void *)int_io_pin);
    ESP_LOGI(TAG, "ISR install Success");
}

/* GET */
void RTC::get_temperature(void)
{
    uint8_t temp_add = REG_TEMPERATURE_H;
    uint8_t temp_buff[2] = {0};
    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, &temp_add, 1, temp_buff, 2, -1));
    temperature = (temp_buff[0] >> 7 == 1 ? -1 : 1) * (((temp_buff[0] & 0x7F) << 2) | (temp_buff[1] >> 6)) * 0.25;
}

struct tm *RTC::get_time(void)
{
    uint8_t temp_add = REG_SECONDS;
    uint8_t temp_buff[7] = {0};
    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, &temp_add, 1, temp_buff, sizeof(temp_buff), -1));
    time.tm_sec = temp_buff[0];
    time.tm_min = temp_buff[1];
    time.tm_hour = temp_buff[2];
    time.tm_wday = temp_buff[3];
    time.tm_mday = temp_buff[4];
    time.tm_mon = temp_buff[5];
    time.tm_year = temp_buff[6];

    return &time;
}

void RTC::get_config(void)
{
    uint8_t temp_add = REG_CONTROL;
    uint8_t temp_buff = 0x00;

    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, &temp_add, 1, &temp_buff, sizeof(temp_buff), -1));
    std::memcpy(&reg.control, &temp_buff, sizeof(temp_buff));
}

void RTC::get_status(void)
{
    uint8_t temp_add = REG_STATUS;
    uint8_t temp_buff = 0x00;

    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, &temp_add, 1, &temp_buff, sizeof(temp_buff), -1));
    std::memcpy(&reg.status, &temp_buff, sizeof(temp_buff));
}

/* SET */
void RTC::set_config(void)
{
    uint8_t temp_add = REG_CONTROL;
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, &temp_add, 1, -1));
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, (uint8_t *)&reg.control, sizeof(reg.control), -1));
}

void RTC::set_status(void)
{
    uint8_t temp_add = REG_STATUS;
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, &temp_add, 1, -1));
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, (uint8_t *)&reg.status, sizeof(reg.status), -1));
}

void RTC::set_time(tm *new_time)
{
    uint8_t temp_add = REG_SECONDS;
    uint8_t temp_buff[7] = {0};

    i2c_master_transmit_multi_buffer_info_t multi_buff[2] = {
        {.write_buffer = &temp_add, .buffer_size = sizeof(temp_add)},
        {.write_buffer = temp_buff, .buffer_size = sizeof(temp_buff)},
    };

    temp_buff[0] = new_time->tm_sec;
    temp_buff[1] = new_time->tm_min;
    temp_buff[2] = new_time->tm_hour;
    temp_buff[3] = new_time->tm_wday;
    temp_buff[4] = new_time->tm_mday;
    temp_buff[5] = new_time->tm_mon;
    temp_buff[6] = new_time->tm_year;

    ESP_ERROR_CHECK(i2c_master_multi_buffer_transmit(dev_handle, multi_buff, sizeof(multi_buff) / sizeof(i2c_master_transmit_multi_buffer_info_t), -1));
}

/* USER FUNCTION */
void RTC::reset(void)
{
    gpio_set_level(rst_io_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(rst_io_pin, 1);
    ESP_LOGI(TAG, "reset");
}

std::string RTC::getTimestamp()
{
    const size_t bufferSize = 64;
    char buffer[bufferSize];

    size_t resultSize = std::strftime(buffer, bufferSize, "%Y-%m-%d-%H-%M-%S", get_time());

    // 将缓冲区内容转换为 std::string
    return std::string(buffer, resultSize);
}
