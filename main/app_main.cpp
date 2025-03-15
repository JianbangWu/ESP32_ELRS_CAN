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

#include "elrs.hpp"
#include "ds3231m.hpp"
#include "sd_card.hpp"
#include "beep.hpp"
#include "uart_redirect.hpp"

RTC *ds3231_obj = nullptr;
SDCard *sd_obj = nullptr;
ELRS *elrs_obj = nullptr;
Buzzer *buzzer_obj = nullptr;

extern "C" void app_main(void)
{

    ds3231_obj = new RTC();
    sd_obj = new SDCard();
    // elrs_obj = new ELRS();
    buzzer_obj = new Buzzer();

    // 重定向 std::cout 到 USB Serial/JTAG
    redirectStdoutToUsbSerial();

    // std::string directory = "/sdcard"; // SD 卡挂载的根目录

    // std::cout << "TimeStamp: " << ds3231_obj->getTimestamp() << std::endl;

    // // 获取文件列表
    // std::vector<std::string> fileList = SDCard::getFileList(directory);

    // // 打印文件列表
    // std::cout << "Files in " << directory << ":" << std::endl;

    // for (const std::string &file : fileList)
    // {
    //     std::cout << file << std::endl;
    // }

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
