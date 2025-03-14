idf_component_register(SRCS "sd_card.cpp"
                    REQUIRES fatfs sdmmc driver
                    PRIV_REQUIRES beep
                    INCLUDE_DIRS "include")
