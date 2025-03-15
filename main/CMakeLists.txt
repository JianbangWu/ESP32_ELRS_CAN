idf_component_register(SRCS "app_main.cpp"    
                    PRIV_REQUIRES sd_card elrs ds3231m beep ntp_server uart_redirect
                    INCLUDE_DIRS "")
    
