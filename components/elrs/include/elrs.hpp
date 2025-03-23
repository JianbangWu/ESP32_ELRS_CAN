#pragma once

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <ctime>
#include <array>
#include <vector>

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/queue.h"

    class ELRS
    {
    private:
        const char *TAG = "ELRS";
        const uint8_t POLY = 0xD5;
        std::array<uint8_t, 256> lut{};

        const uint8_t HEADER[3] = {0xC8, 0x18, 0x16};

        QueueHandle_t &_tx_queue; // 发送消息队列

        const uart_port_t _port;

        const uint32_t _can_id;
        QueueHandle_t elrs_queue = nullptr;

        double _fps = 0;
        int frame_count = 0;
        std::array<uint16_t, 16> channels;

        std::array<uint8_t, 256> generate_crc_lut()
        {
            for (size_t idx = 0; idx < 256; ++idx)
            {
                uint8_t crc = idx;
                for (int i = 0; i < 8; ++i)
                {
                    crc = (crc & 0x80) ? (crc << 1) ^ POLY : (crc << 1);
                }
                lut[idx] = crc;
            }
            return lut;
        }

        const std::array<uint8_t, 256> CRC_LUT = generate_crc_lut();
        void draw_bar(char *buffer, uint16_t value, uint16_t max_value = 2000) const;
        void parse_channels(const uint8_t *data);
        bool check_crc(const uint8_t *data, size_t len) const;
        void process_packet(const uint8_t *data, std::time_t &last_time);
        void update_frame_rate(std::time_t &last_time);
        void rx_task(void);

    public:
        ELRS(QueueHandle_t &tx_queue,
             uint32_t CAN_ID,
             uart_port_t port = UART_NUM_1,
             gpio_num_t rx_pin = GPIO_NUM_2,
             gpio_num_t tx_pin = GPIO_NUM_1,
             int baudrate = 420000,
             uint16_t tx_buffer_size = 512,
             uint16_t rx_buffer_size = 512,
             uint8_t uart_queen_size = 10);
        ~ELRS();

        std::array<uint16_t, 16> &get_channels(void)
        {
            return channels;
        }

        // 返回 channels 的起始迭代器
        std::array<uint16_t, 16>::const_iterator begin() const
        {
            return channels.begin();
        }

        // 返回 channels 的结束迭代器
        std::array<uint16_t, 16>::const_iterator end() const
        {
            return channels.end();
        }
    };

#ifdef __cplusplus
}
#endif
