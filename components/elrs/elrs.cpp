#include <stdio.h>
#include <cstring>
#include <algorithm>

#include "elrs.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

static const uint32_t StackSize = 1024 * 7;

static const size_t BUFFER_SIZE = 64;
static const size_t PACKET_SIZE = 26;
static const size_t BAR_LENGTH = 50;

void ELRS::rx_task(void) {
    std::vector<uint8_t> buffer;

    std::time_t last_time = std::time(nullptr);

    uart_event_t event;

    while (1) {
        if (xQueueReceive(elrs_queue, (void*)&event, (TickType_t)portMAX_DELAY)) {
            if (event.type == UART_DATA) {
                uint8_t temp_buffer[BUFFER_SIZE];
                int bytes_read = uart_read_bytes(port, temp_buffer, BUFFER_SIZE, pdMS_TO_TICKS(10));
                if (bytes_read <= 0)
                    continue;
                buffer.insert(buffer.end(), temp_buffer, temp_buffer + bytes_read);

                while (buffer.size() >= PACKET_SIZE) {
                    auto it = std::search(buffer.begin(), buffer.end(), std::begin(HEADER), std::end(HEADER));
                    if (it == buffer.end() || std::distance(it, buffer.end()) < PACKET_SIZE) {
                        buffer.erase(buffer.begin(), buffer.begin() + 1); // 丢弃无效数据
                        continue;
                    }

                    size_t start_idx = std::distance(buffer.begin(), it);
                    if (check_crc(buffer.data() + start_idx, PACKET_SIZE)) {
                        process_packet(buffer.data() + start_idx, last_time);
                        ESP_LOGI(TAG, "ELRS FPS := %.2f", fps);
                        buffer.erase(buffer.begin(), buffer.begin() + start_idx + PACKET_SIZE);
                    }
                    else {
                        buffer.erase(buffer.begin(), buffer.begin() + start_idx + 1);
                    }
                }
            }
        }
    }
}

ELRS::ELRS(uart_port_t port, gpio_num_t rx_pin, gpio_num_t tx_pin, int baudrate,
    uint16_t tx_buffer_size, uint16_t rx_buffer_size, uint8_t uart_queen_size) :
    port(port) {

    ESP_ERROR_CHECK(uart_driver_install(port, rx_buffer_size, tx_buffer_size, uart_queen_size, &elrs_queue, 0));

    const uart_config_t uart_config{
        .baud_rate = baudrate,
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

    ESP_ERROR_CHECK(uart_param_config(port, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(port, rx_pin, tx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    auto task_func = [](void* arg) {
        ELRS* instance = static_cast<ELRS*>(arg);
        instance->rx_task(); // 调用类的成员函数
        };

    xTaskCreatePinnedToCore(task_func, "elrs_rx_task", StackSize, this, configMAX_PRIORITIES - 1, nullptr, 1);
}

ELRS::~ELRS() {}

bool ELRS::check_crc(const uint8_t* data, size_t len) const {
    uint8_t crc = 0;
    for (size_t i = 2; i < len - 1; ++i) {
        crc = CRC_LUT[crc ^ data[i]];
    }
    return crc == data[len - 1];
}

void ELRS::update_frame_rate(std::time_t& last_time) {
    std::time_t current_time = std::time(nullptr);
    double elapsed_time = difftime(current_time, last_time);
    if (elapsed_time >= 1.0) {
        fps = frame_count / elapsed_time;
        frame_count = 0;
        last_time = current_time;
#ifdef DEBUG_ENABLE_SERIAL_OUTPUT
        printf("============================================================\n");
        printf("| 帧率: %.2f FPS\n", fps);
#endif
    }
}

void ELRS::process_packet(const uint8_t* data, std::time_t& last_time) {

    parse_channels(data);
    frame_count++;
#ifdef DEBUG_ENABLE_SERIAL_OUTPUT
    char bar_buffer[BAR_LENGTH + 1];
    printf("\033[H"); // 清屏
    std::time_t t = std::time(nullptr);
    std::tm* tm_info = std::localtime(&t);
    char time_buf[9];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
    printf("时间: %s\n", time_buf);
    printf("============================================================\n");
    printf("|  原始数据: ");
    for (size_t i = 0; i < PACKET_SIZE; ++i) {
        printf("%02X ", data[i]);
    }
    printf("\n------------------------------------------------------------\n");
    printf("| 通道 |  数值  | 进度条\n");
    printf("------------------------------------------------------------\n");

    for (size_t i = 0; i < 16; ++i) {
        draw_bar(bar_buffer, channels[i]);
        printf("| ch%2zu  | %5u  | [%s]\n", i + 1, channels[i], bar_buffer);
    }
#endif
    update_frame_rate(last_time);
}

void ELRS::draw_bar(char* buffer, uint16_t value, uint16_t max_value) const {
    size_t bar_fill = std::max<size_t>(1, (value * BAR_LENGTH) / max_value);
    size_t bar_empty = BAR_LENGTH - bar_fill;
    memset(buffer, '=', bar_fill);
    memset(buffer + bar_fill, ' ', bar_empty);
    buffer[BAR_LENGTH] = '\0';
}

void ELRS::parse_channels(const uint8_t* data) {
    for (int i = 0; i < 16; ++i) {
        int bit_position = i * 11;
        int byte_index = 3 + (bit_position / 8);
        int bit_shift = bit_position % 8;
        uint16_t ch_value = (data[byte_index] >> bit_shift) | (data[byte_index + 1] << (8 - bit_shift));
        if (bit_shift > 5) {
            ch_value |= (data[byte_index + 2] << (16 - bit_shift));
        }
        channels[i] = ch_value & 0x07FF;
    }
}
