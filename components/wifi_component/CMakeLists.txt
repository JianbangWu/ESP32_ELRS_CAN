idf_component_register(SRCS "wifi_component.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi esp_netif esp_event console logger)
