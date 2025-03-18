#include "twai_device.hpp"
#include "inttypes.h"

static const uint32_t StackSize = 1024 * 5;

// 后台任务:处理发送消息
void TWAI_Device::tx_task(void* arg) {
    TWAI_Device* device = static_cast<TWAI_Device*>(arg);
    twai_message_t message;
    while (true) {
        if (xQueueReceive(device->_tx_queue, &message, portMAX_DELAY)) {
            twai_transmit(&message, portMAX_DELAY);
        }
    }
}

// 后台任务:处理接收消息
void TWAI_Device::rx_task(void* arg) {
    TWAI_Device* device = static_cast<TWAI_Device*>(arg);
    twai_message_t message;

    while (true) {
        if (twai_receive(&message, portMAX_DELAY) == ESP_OK) {
            // 打印接收到的消息
            ESP_LOGI("TWAI_RX", "Received message: ID=0x%" PRIu32 ", DLC=%d, Data=[%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x]",
                message.identifier, message.data_length_code,
                message.data[0], message.data[1], message.data[2], message.data[3],
                message.data[4], message.data[5], message.data[6], message.data[7]);

            // 将消息放入接收队列
            // xQueueSend(device->_rx_queue, &message, portMAX_DELAY);
        }
    }
}

// 构造函数:初始化TWAI设备
TWAI_Device::TWAI_Device(QueueHandle_t tx_queue,
    QueueHandle_t rx_queue,
    gpio_num_t tx_gpio_num, gpio_num_t rx_gpio_num, gpio_num_t std_gpio_num,
    twai_timing_config_t timing_config,
    twai_filter_config_t filter_config)
    : _tx_gpio_num(tx_gpio_num), _rx_gpio_num(rx_gpio_num), _std_gpio_num(std_gpio_num),
    _timing_config(timing_config), _filter_config(filter_config), _tx_queue(tx_queue), _rx_queue(rx_queue) {

    init( );
    init_io( );
    bus_enbale(true);
}

// 析构函数:清理资源
TWAI_Device::~TWAI_Device( ) {
    deinit( );
}

void TWAI_Device::init_io( ) {
    gpio_config_t io_conf = {};
    /* RST_PIN */
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << _std_gpio_num);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
}

void TWAI_Device::bus_enbale(bool enbale) {
    gpio_set_level(_std_gpio_num, enbale == true ? 0 : 1);
}


// 初始化TWAI驱动和任务
void TWAI_Device::init( ) {
    // 初始化TWAI驱动
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(_tx_gpio_num, _rx_gpio_num, TWAI_MODE_NORMAL);
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &_timing_config, &_filter_config));
    ESP_ERROR_CHECK(twai_start( ));

    // 创建后台任务
    xTaskCreatePinnedToCore(&TWAI_Device::tx_task, "TWAI_TX", StackSize, this, 1, nullptr, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(&TWAI_Device::rx_task, "TWAI_RX", StackSize, this, 1, nullptr, tskNO_AFFINITY);
}

// 清理TWAI驱动和资源
void TWAI_Device::deinit( ) {
    // 停止并卸载TWAI驱动
    ESP_ERROR_CHECK(twai_stop( ));
    ESP_ERROR_CHECK(twai_driver_uninstall( ));
}

// 发送消息到TWAI总线
void TWAI_Device::send_message(const twai_message_t& message) {
    xQueueSend(_tx_queue, &message, portMAX_DELAY);
}

// 从TWAI总线接收消息
bool TWAI_Device::receive_message(twai_message_t& message, TickType_t timeout) {
    return xQueueReceive(_rx_queue, &message, timeout) == pdTRUE;
}
