#pragma once

#include <string.h>
#include <string>

#include <stdio.h>
#include <ctime>

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"
#include "driver/i2c_master.h"

    class RTC
    {
    private:
        const char *TAG = "DS3231M";

        const gpio_num_t rst_io_pin = GPIO_NUM_8;
        const gpio_num_t int_io_pin = GPIO_NUM_18;
        const gpio_num_t clock_out_pin = GPIO_NUM_17;

        i2c_master_bus_config_t i2c_bus_config = {
            .i2c_port = -1,
            .sda_io_num = GPIO_NUM_15,
            .scl_io_num = GPIO_NUM_16,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {0, 0},
        };

        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x68,
            .scl_speed_hz = 100000,
            .scl_wait_us = 0,
            .flags = {0},
        };

        static std::string time_stamp;

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

        void init_io(void);

        void get_temperature(void);
        struct tm *get_time(void);
        void get_config(void);
        void get_status(void);

        void set_config(void);
        void set_status(void);

        void gpio_task(void);

    public:
        i2c_master_bus_handle_t bus_handle = nullptr;
        i2c_master_dev_handle_t dev_handle = nullptr;

        RTC(/* args */);
        ~RTC();

        void reset(void);
        void set_time(tm *new_time);

        std::string getTimestamp();
    };

#ifdef __cplusplus
}
#endif
