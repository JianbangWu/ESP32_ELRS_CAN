idf_component_register(SRCS "twai_device.cpp"
                    REQUIRES driver esp_driver_gpio esp_event log
                    INCLUDE_DIRS "include")
