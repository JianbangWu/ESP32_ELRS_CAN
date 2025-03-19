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
#include "logger.hpp"

USER_CONSOLE *console_obj;

extern "C" void app_main(void)
{
    // 初始化默认事件循环
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK)
    {
        ESP_LOGE("Main", "Failed to create default event loop: %s", esp_err_to_name(ret));
        return;
    }

    SemaphoreHandle_t sntp_sem = xSemaphoreCreateBinary();
    SNTPManager sntp_obj(sntp_sem);
    RTC ds3231_obj(sntp_sem);
    ESP_LOGI("RTC_IMTE", "%s", ds3231_obj.getTimestamp().c_str());

    SDCard sd_obj;
    ELRS elrs_obj;

    QueueHandle_t beep_queue = xQueueCreate(5, sizeof(BeeperMessage));
    Buzzer buzzer_obj(beep_queue);

    QueueHandle_t twai_tx_queue = xQueueCreate(10, sizeof(twai_message_t));
    QueueHandle_t twai_rx_queue = xQueueCreate(10, sizeof(twai_message_t));
    TWAI_Device twai_obj(twai_tx_queue, twai_rx_queue);

    console_obj = new USER_CONSOLE();

    // 创建 LoggerBase 实例，挂载点为 "/sdcard/sensor_data"
    // LoggerBase logger("/sdcard/sensor_data");

    // 初始化日志文件，文件名为 "sensor_log.txt"
    //     if (logger.init("sensor_log2.txt"))
    //     {
    // 记录日志信息
    //         logger.log("Sensor 2: Humidity = 60%");
    //         logger.log("Sensor 1: Temperature = 25.3C");
    //     }
    //     else
    //     {
    //         printf("Failed to initialize logger!\n");
    //     }
    //
    //     logger.shutdown();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
