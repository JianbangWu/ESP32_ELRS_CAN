idf_component_register(SRCS "ntp.c"
                    PRIV_REQUIRES esp_netif nvs_flash protocol_examples_common 
                    INCLUDE_DIRS "include")
