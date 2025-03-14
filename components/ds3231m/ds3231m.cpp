#include <stdio.h>
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

static void gpio_task(void *arg)
{
    gpio_num_t io_num;
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            printf("GPIO[%" PRIu32 "] intr, val: %d\n", (long unsigned int)io_num, gpio_get_level(io_num));
        }
    }
}

ds3231m::ds3231m()
{
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));
    ESP_ERROR_CHECK(i2c_master_bus_add_device((bus_handle), &(dev_cfg), &(dev_handle)));

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

    xTaskCreate(gpio_task, "rtc_int", 2048, NULL, 10, NULL);

    esp_err_t ret = gpio_install_isr_service(0);

    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        printf("ISR install fail\n");
        return;
    }

    gpio_isr_handler_add(int_io_pin, gpio_isr_handler, (void *)int_io_pin);
}

ds3231m::~ds3231m()
{
    ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
    printf("I2C Bus Is Free! \r\n");
}

void ds3231m::get_rtc_temperature(void)
{
    uint8_t temp_add = REG_TEMPERATURE_H;
    uint8_t temp_buff[2] = {0};
    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, &temp_add, 1, temp_buff, 2, -1));
    temperature = (temp_buff[0] >> 7 == 1 ? -1 : 1) * (((temp_buff[0] & 0x7F) << 2) | (temp_buff[1] >> 6)) * 0.25;

    printf("Temp = %f \r\n", temperature);
}

void ds3231m::get_rtc_time(void)
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
}

void ds3231m::get_rtc_config(void)
{
    uint8_t temp_add = REG_CONTROL;
    uint8_t temp_buff = 0x00;

    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, &temp_add, 1, &temp_buff, sizeof(temp_buff), -1));
    memcpy(&ds3231m_config.control, &temp_buff, sizeof(temp_buff));

    printf("REG_CONTROL :\r\n EOSC:%d,BBSQW:%d,CONV:%d,INTCN:%d,A2IE:%d,A1IE:%d \r\n", ds3231m_config.control.close_oscillator,
           ds3231m_config.control.set_bat_backed_square_wave_enable, ds3231m_config.control.set_temperature_convert, ds3231m_config.control.interrupt_pin_enable, ds3231m_config.control.alarm2_enable, ds3231m_config.control.alarm1_enable);
}

void ds3231m::get_rtc_status(void)
{
    uint8_t temp_add = REG_STATUS;
    uint8_t temp_buff = 0x00;

    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, &temp_add, 1, &temp_buff, sizeof(temp_buff), -1));
    memcpy(&ds3231m_config.status, &temp_buff, sizeof(temp_buff));

    printf("REG_STATUS :\r\n OSF:%d,EN32KHZ:%d,BSY:%d,A2F:%d,A1F:%d \r\n", ds3231m_config.status.oscillator_is_stop,
           ds3231m_config.status.set_32khz_output, ds3231m_config.status.is_busy, ds3231m_config.status.alarm2_flag, ds3231m_config.status.alarm1_flag);
}

void ds3231m::set_rtc_config(void)
{
    uint8_t temp_add = REG_CONTROL;
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, &temp_add, 1, -1));
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, (uint8_t *)&ds3231m_config.control, sizeof(ds3231m_config.control), -1));
}

void ds3231m::set_rtc_status(void)
{
    uint8_t temp_add = REG_STATUS;
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, &temp_add, 1, -1));
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, (uint8_t *)&ds3231m_config.status, sizeof(ds3231m_config.status), -1));
}

void ds3231m::set_rtc_time(tm *new_time)
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

void ds3231m::print_time(void)
{
    strftime(buffer, sizeof(buffer), "%Y-%m-%d-%A %H:%M:%S", &time);
    printf("rtc_time:= %s \r\n", buffer);
}

void ds3231m::reset(void)
{
    gpio_set_level(rst_io_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(rst_io_pin, 1);
    ESP_LOGI(TAG, "reset");
}
