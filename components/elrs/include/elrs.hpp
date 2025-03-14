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

    class elrs
    {
    private:
        const uart_config_t uart_config = {
            .baud_rate = CONFIG_ELRS_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };

        const char *TAG = "ELRS";

        const uint8_t POLY = 0xD5;

        std::array<uint8_t, 256> lut{};

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

        void update_frame_rate(int &frame_count, std::time_t &last_time) const;
        void draw_bar(char *buffer, uint16_t value, uint16_t max_value = 2000) const;
        void parse_channels(const uint8_t *data, uint16_t *channels) const;

    public:
        const int RX_BUF_SIZE = CONFIG_ELRS_UART_RX_BUFF_SIZE;
        const int TX_BUF_SIZE = CONFIG_ELRS_UART_TX_BUFF_SIZE;

        bool check_crc(const uint8_t *data, size_t len) const;
        void process_packet(const uint8_t *data, int &frame_count, std::time_t &last_time);

        const size_t BUFFER_SIZE = 64;
        const size_t PACKET_SIZE = 26;
        const size_t BAR_LENGTH = 50;
        const uint8_t HEADER[3] = {0xC8, 0x18, 0x16};

        uint16_t channels[16];

        QueueHandle_t elrs_queue = nullptr;

        elrs();
        ~elrs();
    };

#ifdef __cplusplus
}
#endif
