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

    std::chrono::time_point<std::chrono::steady_clock> origin_time = std::chrono::steady_clock::now();

    // 初始化默认事件循环
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK)
    {
        ESP_LOGE("Main", "Failed to create default event loop: %s", esp_err_to_name(ret));
        return;
    }

    QueueHandle_t twai_tx_queue = xQueueCreate(10, sizeof(twai_message_t));
    QueueHandle_t twai_rx_queue = xQueueCreate(10, sizeof(twai_message_t));
    TWAI_Device twai_obj(twai_tx_queue, twai_rx_queue, origin_time);

    EventGroupHandle_t wifi_event_group = xEventGroupCreate();

    SemaphoreHandle_t sntp_sem = xSemaphoreCreateBinary();
    SNTPManager sntp_obj(sntp_sem, wifi_event_group);
    RTC ds3231_obj(sntp_sem);

    printf("\033[92;45m RTC_IMTE: CST-8:=%s \033[0m \r\n", ds3231_obj.get_cst8_time().c_str());

    SDCard sd_obj;
    ELRS elrs_obj;

    QueueHandle_t beep_queue = xQueueCreate(5, sizeof(BeeperMessage));
    Buzzer buzzer_obj(beep_queue);

    console_obj = new USER_CONSOLE(CmdFilesystem::_prompt_change_sem, CmdFilesystem::_current_path, WiFiComponent::_wifi_state);

    WiFiComponent wifi(CmdFilesystem::_prompt_change_sem, wifi_event_group);
    wifi.registerConsoleCommands();
    CmdSystem::registerSystem();
    CmdNVS::registerNVS();
    CmdFilesystem::registerCommands();
    //
    //     auto current_time = std::chrono::steady_clock::now();
    //     auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(current_time - origin_time).count();
    //
    //     ESP_LOGI("MAIN", "Boot = %lld", timestamp);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
