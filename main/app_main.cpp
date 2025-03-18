/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <iostream>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sntp_service.hpp"
#include "elrs.hpp"
#include "ds3231m.hpp"
#include "sd_card.hpp"
#include "beep.hpp"

#include "user_console.hpp"
#include "twai_device.hpp"

SNTPManager* ntp_obj = nullptr;
RTC* ds3231_obj = nullptr;
SDCard* sd_obj = nullptr;
ELRS* elrs_obj = nullptr;
Buzzer* buzzer_obj = nullptr;
USER_CONSOLE* console_obj = nullptr;
TWAI_Device* twai_obj = nullptr;

extern "C" void app_main(void) {

    QueueHandle_t sntp_queue = xQueueCreate(10, sizeof(uint8_t));
    QueueHandle_t twai_tx_queue = xQueueCreate(10, sizeof(twai_message_t));
    QueueHandle_t twai_rx_queue = xQueueCreate(10, sizeof(twai_message_t));

    // 初始化默认事件循环
    esp_err_t ret = esp_event_loop_create_default( );
    if (ret != ESP_OK) {
        ESP_LOGE("Main", "Failed to create default event loop: %s", esp_err_to_name(ret));
        return;
    }

    ntp_obj = new SNTPManager(sntp_queue);
    ds3231_obj = new RTC(sntp_queue);

    sd_obj = new SDCard( );
    ESP_LOGI("RTC_IMTE", "%s", ds3231_obj->getTimestamp( ).c_str( ));

    elrs_obj = new ELRS( );
    buzzer_obj = new Buzzer( );

    console_obj = new USER_CONSOLE( );
    twai_obj = new TWAI_Device(twai_tx_queue, twai_rx_queue);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
