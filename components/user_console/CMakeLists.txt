idf_component_register(SRCS "user_console.cpp"
                    REQUIRES nvs_flash console fatfs sd_card
                    INCLUDE_DIRS "include")
