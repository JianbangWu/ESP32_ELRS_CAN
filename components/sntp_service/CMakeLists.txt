idf_component_register(SRCS "sntp_service.cpp"
                    PRIV_REQUIRES esp_netif console esp_event
                    INCLUDE_DIRS "include")
