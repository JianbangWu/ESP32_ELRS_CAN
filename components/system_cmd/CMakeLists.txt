idf_component_register(SRCS "system_cmd.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES console spi_flash driver esp_driver_gpio)
