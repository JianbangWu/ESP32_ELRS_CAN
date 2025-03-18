idf_component_register(SRCS "sd_card.cpp"
                    REQUIRES fatfs sdmmc json ds3231m console esp_driver_gpio
                    INCLUDE_DIRS "include")
