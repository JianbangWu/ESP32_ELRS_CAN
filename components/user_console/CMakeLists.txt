idf_component_register(SRCS "user_console.cpp"
                    REQUIRES nvs_flash console esp_vfs_console fatfs sd_card esp_wifi wifi_component
                    INCLUDE_DIRS "include")
