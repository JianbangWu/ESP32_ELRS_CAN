idf_component_register(SRCS "app_main.cpp"    
                    PRIV_REQUIRES sd_card elrs ds3231m beep sntp_service user_console twai_device
                    logger
                    INCLUDE_DIRS ".")
    
