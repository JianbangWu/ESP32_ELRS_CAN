#pragma once

#include <string>
#include <stdio.h>
#include <ctime>

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"

    class RTC
    {
    private:
        const char *TAG = "DS3231M";

        SemaphoreHandle_t _isr_sem;
        SemaphoreHandle_t _sntp_sem;

        gpio_num_t _rst_io_pin;
        gpio_num_t _int_io_pin;
        gpio_num_t _clock_out_pin;

        i2c_master_bus_handle_t bus_handle = nullptr;
        i2c_master_dev_handle_t dev_handle = nullptr;

        struct tm now_time;
        struct tm alarm1;
        struct tm alarm2; /* NO SECONDS */
        float temperature;

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

        } reg;

        static void IRAM_ATTR gpio_isr_handler(void *arg);

        void init_io(void);

        void get_temperature(void);
        struct tm *get_time(void);
        void get_config(void);
        void get_status(void);

        void set_config(void);
        void set_status(void);

        void task(void);

    public:
        RTC(QueueHandle_t &rtc_handle,
            gpio_num_t rst_pin = GPIO_NUM_8,
            gpio_num_t init_pin = GPIO_NUM_18,
            gpio_num_t _clock_out_pin = GPIO_NUM_17,
            gpio_num_t sda_pin = GPIO_NUM_15,
            gpio_num_t scl_pin = GPIO_NUM_16,
            uint16_t dev_addr = 0x68);

        ~RTC();

        void reset(void);

        void set_time(tm *new_time);

        std::string getTimestamp();
    };

#ifdef __cplusplus
}
#endif
