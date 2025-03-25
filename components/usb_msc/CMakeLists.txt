idf_component_register(SRCS "usb_msc.cpp"
                    REQUIRES esp_tinyusb esp_driver_gpio nvs_flash console sdmmc sd_card
                    INCLUDE_DIRS "include")
