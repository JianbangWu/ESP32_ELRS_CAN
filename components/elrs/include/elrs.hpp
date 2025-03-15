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

#include "driver/uart.h"
#include "freertos/queue.h"

    class ELRS
    {
    private:
        const uart_config_t uart_config{
            .baud_rate = CONFIG_ELRS_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
            .source_clk = UART_SCLK_DEFAULT,
            .flags{
                .allow_pd = 0,
                .backup_before_sleep = 0,
            },
        };

        const char *TAG = "ELRS";
        const uint8_t POLY = 0xD5;
        std::array<uint8_t, 256> lut{};
        const size_t BUFFER_SIZE = 64;
        const size_t PACKET_SIZE = 26;
        const size_t BAR_LENGTH = 50;
        const uint8_t HEADER[3] = {0xC8, 0x18, 0x16};
        const int RX_BUF_SIZE = CONFIG_ELRS_UART_RX_BUFF_SIZE;
        const int TX_BUF_SIZE = CONFIG_ELRS_UART_TX_BUFF_SIZE;
        QueueHandle_t elrs_queue = nullptr;

        double fps = 0;
        int frame_count = 0;
        static std::array<uint16_t, 16> channels;

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
        void rx_task();

    public:
        ELRS();
        ~ELRS();

        const std::array<uint16_t, 16> &get_channels(void)
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
