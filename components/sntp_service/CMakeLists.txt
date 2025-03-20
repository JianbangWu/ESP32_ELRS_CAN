idf_component_register(SRCS "sntp_service.cpp"
                    REQUIRES esp_netif console esp_event
                    PRIV_REQUIRES wifi_component
                    INCLUDE_DIRS "include")
