#include <stdio.h>
#include <cstring>
#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "ds3231m.hpp"

static const uint32_t StackSize = 1024 * 5;

void IRAM_ATTR RTC::gpio_isr_handler(void *arg)
{
    RTC *instance = static_cast<RTC *>(arg);
    xSemaphoreGiveFromISR(instance->_isr_sem, nullptr);
}

void RTC::task(void)
{
    while (1)
    {
        if (xSemaphoreTake(_isr_sem, pdMS_TO_TICKS(200)))
        {
            ESP_LOGI(TAG, "INT Task");
        }
        if (xSemaphoreTake(_sntp_sem, pdMS_TO_TICKS(500)))
        {
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            set_time(&timeinfo);
            reg.status.oscillator_is_stop = 0;
            set_status();
        }
    }
}

RTC::RTC(SemaphoreHandle_t &sntp_sem,
         gpio_num_t rst_pin,
         gpio_num_t init_pin,
         gpio_num_t clock_out_pin,
         gpio_num_t sda_pin,
         gpio_num_t scl_pin,
         uint16_t dev_addr) : _sntp_sem(sntp_sem),
                              _rst_io_pin(rst_pin),
                              _int_io_pin(init_pin),
                              _clock_out_pin(clock_out_pin)

{
    _isr_sem = xSemaphoreCreateBinary();

    if (_sntp_sem == nullptr)
    {
        ESP_LOGE(TAG, "Invalid queue handle provided");
        return; // 或者抛出异常
    }

    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = -1,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {0, 0},
    };

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = 100000,
        .scl_wait_us = 0,
        .flags = {0},
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));
    ESP_ERROR_CHECK(i2c_master_bus_add_device((bus_handle), &(dev_cfg), &(dev_handle)));
    init_io();
    get_config();
    if (reg.status.oscillator_is_stop == 1)
    {
        ESP_LOGE(TAG, "RTC BAT Was Error!");
        ESP_LOGE(TAG, "Time Need Calibrate!");
    }
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
    io_conf.pin_bit_mask = (1ULL << _rst_io_pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    /* INT PIN */
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << _int_io_pin);
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    auto task_func = [](void *arg)
    {
        RTC *instance = static_cast<RTC *>(arg);
        instance->task(); // 调用类的成员函数
    };

    xTaskCreate(task_func, "ds3231m", StackSize, this, 10, NULL);

    esp_err_t ret = gpio_install_isr_service(0);

    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Detect Pin ISR Init Failed! 0x%x", ret);
        return;
    }

    if (ESP_OK == (gpio_isr_handler_add(_int_io_pin, gpio_isr_handler, this)))
    {
        ESP_LOGI(TAG, "ISR install Success");
    }
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
    now_time.tm_sec = temp_buff[0];
    now_time.tm_min = temp_buff[1];
    now_time.tm_hour = temp_buff[2];
    now_time.tm_wday = temp_buff[3];
    now_time.tm_mday = temp_buff[4];
    now_time.tm_mon = temp_buff[5];
    now_time.tm_year = temp_buff[6];

    return &now_time;
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

    ESP_LOGI(TAG, "Time Calibrated");
}

/* USER FUNCTION */
void RTC::reset(void)
{
    gpio_set_level(_rst_io_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(_rst_io_pin, 1);
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
