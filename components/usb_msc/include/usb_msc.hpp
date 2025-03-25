#pragma once

#include <string>
#include "nvs_handle.hpp"
#include "sd_card.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include "nvs.h"
#include "sdmmc_cmd.h"

    class USB_MSC
    {
    private:
        const char *TAG = "USB_MSC";
        SDCard sd_obj;

    public:
        USB_MSC();
        ~USB_MSC();

        esp_err_t storage_init_sdmmc(void);

        void _mount(void);

        static int get_init_msc_key(const char *ns_name = "storage");

        static void set_init_msc_key(uint8_t new_val, const char *ns_name = "storage");

        static int console_mount(int argc, char **argv);

        static void registerMount();

        static const char *_init_msc_key;
    };

#ifdef __cplusplus
}
#endif