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
#include "wifi_component.hpp"
#include "user_console.hpp"
#include "twai_device.hpp"
#include "logger.hpp"
#include "system_cmd.hpp"
#include "nvs_component.hpp"
#include "filesystem_cmd.hpp"

USER_CONSOLE *console_obj;

extern "C" void app_main(void)
{

    setenv("TZ", "CST-8", 1);
    tzset();

    /* 启动时间 */
    std::chrono::time_point<std::chrono::steady_clock> origin_time = std::chrono::steady_clock::now();

    // 初始化默认事件循环
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK)
    {
        ESP_LOGE("Main", "Failed to create default event loop: %s", esp_err_to_name(ret));
        return;
    }
    /* 蜂鸣器 */
    QueueHandle_t beep_queue = xQueueCreate(5, sizeof(BeeperMessage));
    Buzzer buzzer_obj(beep_queue);
    /* TWAI外设初始化 */
    QueueHandle_t twai_tx_queue = xQueueCreate(10, sizeof(twai_message_t));
    QueueHandle_t twai_rx_queue = xQueueCreate(10, sizeof(twai_message_t));
    TWAI_Device twai_obj(beep_queue, twai_tx_queue, twai_rx_queue, origin_time);
    /* WIFI事件 */
    EventGroupHandle_t wifi_event_group = xEventGroupCreate();
    /* SNTP服务 */
    SemaphoreHandle_t sntp_sem = xSemaphoreCreateBinary();
    SNTPManager sntp_obj(sntp_sem, wifi_event_group);
    /* RCT服务 */
    RTC ds3231_obj(beep_queue, sntp_sem);
    printf("\033[92;45m RTC_IMTE: CST-8:=%s \033[0m \r\n", ds3231_obj.get_cst8_time().c_str());
    /* SD-Card */
    SDCard sd_obj;
    /* ELRS解析业务 */
    ELRS elrs_obj(twai_tx_queue, 0x12345678UL);
    /* 串口终端控制台 */
    console_obj = new USER_CONSOLE(CmdFilesystem::_prompt_change_sem, CmdFilesystem::_current_path, WiFiComponent::_wifi_state);
    /* WIFI业务初始化 */
    WiFiComponent wifi(CmdFilesystem::_prompt_change_sem, wifi_event_group);
    wifi.registerConsoleCommands();

    /* 注册终端命令 */
    CmdSystem::registerSystem();
    CmdNVS::registerNVS();
    CmdFilesystem::registerCommands();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
