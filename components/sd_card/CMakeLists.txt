idf_component_register(SRCS "sd_card.cpp"
                    REQUIRES fatfs sdmmc driver json
                    PRIV_REQUIRES beep 
                    INCLUDE_DIRS "include")
