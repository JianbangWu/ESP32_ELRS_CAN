idf_component_register(SRCS "app_main.cpp"    
                    PRIV_REQUIRES sd_card elrs ds3231m beep sntp_service user_console twai_device
                    logger wifi_component system_cmd nvs_component filesystem_cmd usb_msc
                    INCLUDE_DIRS ".")
    
