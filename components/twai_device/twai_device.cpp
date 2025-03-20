#include "twai_device.hpp"
#include "inttypes.h"

#include <ctime>
#include <time.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>

#include "logger.hpp"

static const uint32_t StackSize = 1024 * 5;

// 后台任务:处理发送消息
void TWAI_Device::tx_task(void *arg)
{
    TWAI_Device *device = static_cast<TWAI_Device *>(arg);
    twai_message_t message;
    while (true)
    {
        if (xQueueReceive(device->_tx_queue, &message, pdMS_TO_TICKS(50)))
        {
            twai_transmit(&message, portMAX_DELAY);
        }
    }
}

// 后台任务:处理接收消息
void TWAI_Device::rx_task(void *arg)
{
    TWAI_Device *device = static_cast<TWAI_Device *>(arg);

    twai_message_t message;

    while (true)
    {
        if (twai_receive(&message, portMAX_DELAY) == ESP_OK)
        {
            // 打印接收到的消息
            printf("TWAI_RX:Received message: ID=0x%08" PRIu32 ", DLC=%2d, Data=[%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x]\r",
                   message.identifier, message.data_length_code,
                   message.data[0], message.data[1], message.data[2], message.data[3],
                   message.data[4], message.data[5], message.data[6], message.data[7]);

            // 将消息放入接收队列
            xQueueSend(device->_rx_queue, &message, portMAX_DELAY);
        }
    }
}

void TWAI_Device::rx_log(void *arg)
{
    TWAI_Device *device = static_cast<TWAI_Device *>(arg);
    twai_message_t message;

    while (true)
    {
        if (xQueueReceive(device->_rx_queue, &message, pdMS_TO_TICKS(2000)))
        {

            std::time_t now = std::time(nullptr);

            if (false == device->_twai_logger.get_init_state())
            {
                device->_twai_logger.init(device->get_date(now) + "/" + device->get_timestamp(now) + ".asc");
            }

            device->_twai_logger.log(device->format_asc(1, message.identifier, "Rx", message.data_length_code, &message.data[0]), 50000);
        }
        else
        {
            if (true == device->_twai_logger.get_init_state())
            {
                device->_twai_logger.shutdown();
                ESP_LOGI(device->TAG, "Bus Timeout, Shutdown File");
            }
        }
    }
}

std::string TWAI_Device::format_asc(int channel, uint32_t canId, const std::string &direction, int dataLength, const uint8_t *data)
{
    auto current_time = std::chrono::steady_clock::now();

    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(current_time - _origin_time).count();

    // 将微秒转换为秒
    double timestamp_seconds = static_cast<double>(timestamp) / 1'000'000.0;

    std::ostringstream oss;

    // Format timestamp (6 decimal places)
    oss << std::fixed << std::setprecision(6) << timestamp_seconds << " ";

    // Format channel
    oss << channel << " ";

    // Format CAN ID (8-digit hex with 'x' suffix)
    oss << std::hex << std::setw(8) << std::setfill('0') << canId << "x ";

    // Format direction and data length
    oss << direction << " d " << std::dec << dataLength << " ";

    for (int i = 0; i < dataLength; ++i)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
    }

    // Remove the trailing space and return the formatted string
    std::string result = oss.str();
    if (!result.empty() && result.back() == ' ')
    {
        result.pop_back(); // Remove the last space
    }

    return result;
}

std::string TWAI_Device::get_timestamp(std::time_t &time)
{
    const size_t bufferSize = 64;
    char buffer[bufferSize];

    std::strftime(buffer, bufferSize, "%Y_%m_%d-%H_%M_%S", std::localtime(&time));

    return std::string(buffer);
}

std::string TWAI_Device::get_date(std::time_t &time)
{
    const size_t bufferSize = 64;
    char buffer[bufferSize];

    std::strftime(buffer, bufferSize, "%m-%d", std::localtime(&time));

    return std::string(buffer);
}

// 构造函数:初始化TWAI设备
TWAI_Device::TWAI_Device(QueueHandle_t &tx_queue,
                         QueueHandle_t &rx_queue,
                         std::chrono::time_point<std::chrono::steady_clock> &origin_time,
                         gpio_num_t tx_gpio_num,
                         gpio_num_t rx_gpio_num,
                         gpio_num_t std_gpio_num,
                         twai_timing_config_t timing_config,
                         twai_filter_config_t filter_config)
    : _origin_time(origin_time),
      _tx_gpio_num(tx_gpio_num),
      _rx_gpio_num(rx_gpio_num),
      _std_gpio_num(std_gpio_num),
      _timing_config(timing_config),
      _filter_config(filter_config),
      _tx_queue(tx_queue),
      _rx_queue(rx_queue),
      _twai_logger("/sdcard/twai")
{
    init();
    init_io();
    bus_enbale(true);
}

// 析构函数:清理资源
TWAI_Device::~TWAI_Device()
{
    deinit();
}

void TWAI_Device::init_io()
{
    gpio_config_t io_conf = {};
    /* RST_PIN */
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << _std_gpio_num);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
}

void TWAI_Device::bus_enbale(bool enbale)
{
    gpio_set_level(_std_gpio_num, enbale == true ? 0 : 1);
}

// 初始化TWAI驱动和任务
void TWAI_Device::init()
{
    // 初始化TWAI驱动
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(_tx_gpio_num, _rx_gpio_num, TWAI_MODE_NORMAL);
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &_timing_config, &_filter_config));
    ESP_ERROR_CHECK(twai_start());

    // 创建后台任务
    xTaskCreatePinnedToCore(&TWAI_Device::tx_task, "TWAI_TX", StackSize, this, 1, nullptr, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(&TWAI_Device::rx_task, "TWAI_RX", StackSize, this, 1, nullptr, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(&TWAI_Device::rx_log, "rx_logger", StackSize, this, 1, nullptr, tskNO_AFFINITY);
}

// 清理TWAI驱动和资源
void TWAI_Device::deinit()
{
    // 停止并卸载TWAI驱动
    ESP_ERROR_CHECK(twai_stop());
    ESP_ERROR_CHECK(twai_driver_uninstall());
}

// 发送消息到TWAI总线
void TWAI_Device::send_message(const twai_message_t &message)
{
    xQueueSend(_tx_queue, &message, portMAX_DELAY);
}

// 从TWAI总线接收消息
bool TWAI_Device::receive_message(twai_message_t &message, TickType_t timeout)
{
    return xQueueReceive(_rx_queue, &message, timeout) == pdTRUE;
}
