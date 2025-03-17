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
#include "esp_chip_info.h"
#include "esp_system.h"

#include "ntp_service.hpp"
#include "elrs.hpp"
#include "ds3231m.hpp"
#include "sd_card.hpp"
#include "beep.hpp"
#include "uart_redirect.hpp"
#include "user_console.hpp"

SNTPManager *ntp_obj = nullptr;

RTC *ds3231_obj = nullptr;
SDCard *sd_obj = nullptr;
ELRS *elrs_obj = nullptr;
Buzzer *buzzer_obj = nullptr;

USER_CONSOLE *console_obj = nullptr;

extern "C" void app_main(void)
{
    esp_event_loop_create_default();

    ntp_obj = new SNTPManager();
    ds3231_obj = new RTC();
    sd_obj = new SDCard();
    // elrs_obj = new ELRS();
    buzzer_obj = new Buzzer();
    console_obj = new USER_CONSOLE();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
