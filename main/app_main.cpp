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

    // 创建一个 JSON 对象
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "name", "John Doe");
    cJSON_AddNumberToObject(json, "age", 30);
    cJSON_AddBoolToObject(json, "is_student", false);

    // 写入 JSON 数据到文件
    const char *filename = "data.json";
    if (sd_obj->write_json_to_file(filename, json))
    {
        printf("JSON data written to file: %s\n", filename);
    }
    else
    {
        printf("Failed to write JSON data to file.\n");
    }

    // 释放 JSON 对象
    cJSON_Delete(json);

    // 从文件读取 JSON 数据
    cJSON *read_json = sd_obj->read_json_from_file(filename);
    if (read_json)
    {
        printf("JSON data read from file:\n%s\n", cJSON_Print(read_json));
        cJSON_Delete(read_json); // 释放读取的 JSON 对象
    }
    else
    {
        printf("Failed to read JSON data from file.\n");
    }

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
