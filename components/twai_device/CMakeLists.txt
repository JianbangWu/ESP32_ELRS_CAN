idf_component_register(SRCS "twai_device.cpp"
                    REQUIRES driver esp_driver_gpio esp_event logger
                    INCLUDE_DIRS "include")
