#include "usb_msc.hpp"
#include "esp_console.h"
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>

#include "esp_check.h"

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

decltype(USB_MSC::_init_msc_key) USB_MSC::_init_msc_key = "init_msc";

enum
{
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum
{
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN = 0x80,

    EDPT_MSC_OUT = 0x01,
    EDPT_MSC_IN = 0x81,
};

// callback that is delivered when storage is mounted/unmounted by application.
static void storage_mount_changed_cb(tinyusb_msc_event_t *event)
{
    ESP_LOGI("USB_MSC", "Storage mounted to application: %s", event->mount_changed_data.is_mounted ? "Yes" : "No");
}

static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "TinyUSB",                  // 1: Manufacturer
    "TinyUSB Device",           // 2: Product
    "123456",                   // 3: Serials
    "Example MSC",              // 4. MSC
};

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, // This is Espressif VID. This needs to be changed according to Users / Customers
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01};

static uint8_t const msc_fs_configuration_desc[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, 64),
};

USB_MSC::USB_MSC()
{
    ESP_LOGI(TAG, "Initializing storage...");

    //     ESP_ERROR_CHECK(storage_init_sdmmc());
    //
    const tinyusb_msc_sdmmc_config_t config_sdmmc = {
        .card = sd_obj.get_card_handle(),
        .callback_mount_changed = storage_mount_changed_cb, /* First way to register the callback. This is while initializing the storage. */
        .mount_config{.max_files = 5},
    };
    //
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_sdmmc(&config_sdmmc));
    // ESP_ERROR_CHECK(tinyusb_msc_register_callback(TINYUSB_MSC_EVENT_MOUNT_CHANGED, storage_mount_changed_cb));
    //
    //     _mount();

    ESP_LOGI(TAG, "USB MSC initialization");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &descriptor_config,
        .string_descriptor = string_desc_arr,
        .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
        .external_phy = false,
#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor = msc_fs_configuration_desc,
        .hs_configuration_descriptor = msc_hs_configuration_desc,
        .qualifier_descriptor = &device_qualifier,
#else
        .configuration_descriptor = msc_fs_configuration_desc,
#endif // TUD_OPT_HIGH_SPEED
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB MSC initialization DONE");
    set_init_msc_key(0);
}

USB_MSC::~USB_MSC()
{
}

esp_err_t USB_MSC::storage_init_sdmmc(void)
{
    esp_err_t ret = ESP_OK;
    bool host_init = false;

    ESP_LOGI(TAG, "Initializing SDCard");

    sdmmc_host_t host{sd_obj.get_host_handle()};
    sdmmc_slot_config_t slot_config{sd_obj.get_slot_config()};

    sdmmc_card_t *sd_card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    // not using ff_memalloc here, as allocation in internal RAM is preferred
    sd_card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
    ESP_GOTO_ON_FALSE(sd_card, ESP_ERR_NO_MEM, clean, TAG, "could not allocate new sdmmc_card_t");

    ESP_GOTO_ON_ERROR((*host.init)(), clean, TAG, "Host Config Init fail");
    host_init = true;

    ESP_GOTO_ON_ERROR(sdmmc_host_init_slot(host.slot, (const sdmmc_slot_config_t *)&slot_config),
                      clean, TAG, "Host init slot fail");

    while (sdmmc_card_init(&host, sd_card))
    {
        ESP_LOGE(TAG, "The detection pin of the slot is disconnected(Insert uSD card). Retrying...");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    sd_obj.set_card_handle(sd_card);
    return ESP_OK;
clean:
    if (host_init)
    {
        if (host.flags & SDMMC_HOST_FLAG_DEINIT_ARG)
        {
            host.deinit_p(host.slot);
        }
        else
        {
            (*host.deinit)();
        }
    }
    if (sd_card)
    {
        free(sd_card);
        sd_card = NULL;
    }

    return ret;
}

void USB_MSC::_mount(void)
{
    std::string path = sd_obj.get_mount_path();

    ESP_LOGI(TAG, "Mount storage at %s", path.c_str());
    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(path.c_str()));
}

int USB_MSC::get_init_msc_key(const char *ns_name)
{
    esp_err_t ret;
    std::unique_ptr<nvs::NVSHandle>
        nvs_handle = nvs::open_nvs_handle(ns_name, NVS_READWRITE, &ret);

    nvs_type_t nvs_type;
    ret = nvs_handle->find_key(USB_MSC::_init_msc_key, nvs_type);
    uint8_t _msc_key_val;
    if (ESP_ERR_NVS_NOT_FOUND == ret)
    {
        ESP_LOGE("USB_MSC", "_init_msc_key not exit , inited to 0");
        nvs_handle->set_item(USB_MSC::_init_msc_key, 0);
        return 0;
    }
    else if (ESP_OK == ret)
    {
        ret = nvs_handle->get_item(USB_MSC::_init_msc_key, _msc_key_val);
        if (ESP_OK == ret)
        {
            ESP_LOGI("USB_MSC", "_init_msc_key found = %d", _msc_key_val);
            return _msc_key_val;
        }
    }
    return 0;
}

void USB_MSC::set_init_msc_key(uint8_t new_val, const char *ns_name)
{
    esp_err_t ret;
    std::unique_ptr<nvs::NVSHandle>
        nvs_handle = nvs::open_nvs_handle(ns_name, NVS_READWRITE, &ret);
    nvs_handle->set_item(USB_MSC::_init_msc_key, new_val);
}

// unmount storage
int USB_MSC::console_mount(int argc, char **argv)
{
    set_init_msc_key(1);
    ESP_LOGI("USB_MSC", "Restarting");
    esp_restart();
}

void USB_MSC::registerMount()
{
    const esp_console_cmd_t cmd = {
        .command = "usb_mount",
        .help = "Mount SD-Card to USB-MSC",
        .hint = NULL,
        .func = &console_mount,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
