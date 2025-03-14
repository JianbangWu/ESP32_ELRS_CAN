#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <string.h>
#include <stdio.h>
#include <time.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h"

    class ds3231m
    {
    private:
        const char *TAG = "DS3231M";

        i2c_master_bus_config_t i2c_bus_config = {
            .i2c_port = -1,
            .sda_io_num = (gpio_num_t)CONFIG_PIN_SDA,
            .scl_io_num = (gpio_num_t)CONFIG_PIN_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {0, 0},
        };

        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = CONFIG_ADDR_RTC,
            .scl_speed_hz = 100000,
            .scl_wait_us = 0,
            .flags = {0},
        };

        char buffer[80];

        enum reg : uint16_t
        {
            REG_SECONDS = 0x00,
            REG_MINUTES,
            REG_HOURS,
            REG_DAY,
            REG_DATE,
            REG_MOUTH_CENTURY,
            REG_YEAR,
            REG_ALARM1_SECONDS,
            REG_ALARM1_MINUTES,
            REG_ALARM1_HOURS,
            REG_ALARM1_DAY_DATE,
            REG_ALARM2_MINUTES,
            REG_ALARM2_HOURS,
            REG_ALARM2_DAY_DATE,
            REG_CONTROL,
            REG_STATUS,
            REG_AGING_OFFSET,
            REG_TEMPERATURE_H,
            REG_TEMPERATURE_L,
        };

        struct tm time;
        struct tm alarm1;
        struct tm alarm2; /* NO SECONDS */
        float temperature;

        const gpio_num_t rst_io_pin = (gpio_num_t)CONFIG_PIN_RST;
        const gpio_num_t int_io_pin = (gpio_num_t)CONFIG_PIN_INT;
        const gpio_num_t clock_out_pin = (gpio_num_t)CONFIG_PIN_RTC_CLK;

        struct
        {
            struct
            {
                uint8_t alarm1_enable : 1;
                uint8_t alarm2_enable : 1;
                uint8_t interrupt_pin_enable : 1;
                uint8_t : 2;
                uint8_t set_temperature_convert : 1;
                uint8_t set_bat_backed_square_wave_enable : 1;
                uint8_t close_oscillator : 1;
            } control;
            struct
            {
                uint8_t alarm1_flag : 1;
                uint8_t alarm2_flag : 1;
                uint8_t is_busy : 1;
                uint8_t set_32khz_output : 1;
                uint8_t : 3;
                uint8_t oscillator_is_stop : 1;
            } status;

        } ds3231m_config;

    public:
        i2c_master_bus_handle_t bus_handle = nullptr;
        i2c_master_dev_handle_t dev_handle = nullptr;

        ds3231m(/* args */);
        ~ds3231m();
        void get_rtc_temperature(void);
        void get_rtc_time(void);
        void get_rtc_config(void);
        void get_rtc_status(void);
        void set_rtc_config(void);
        void set_rtc_status(void);
        void set_rtc_time(tm *new_time);
        void print_time(void);
        void reset(void);
    };

#ifdef __cplusplus
}
#endif
