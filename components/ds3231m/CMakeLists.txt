idf_component_register(SRCS "ds3231m.cpp"
                    REQUIRES driver esp_wifi beep
                    INCLUDE_DIRS "include")
