/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_system.h"

#include "elrs.hpp"
#include "ds3231m.hpp"
#include "sd_card.hpp"
#include "beep.hpp"

ds3231m *ds3231_obj = nullptr;
sd_card *sd_obj = nullptr;
elrs *elrs_obj = nullptr;
Buzzer *buzzer_obj = nullptr;

extern "C" void app_main(void)
{

    ds3231_obj = new ds3231m();
    sd_obj = new sd_card();
    elrs_obj = new elrs();
    buzzer_obj = new Buzzer();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
