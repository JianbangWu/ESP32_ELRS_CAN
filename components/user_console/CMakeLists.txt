idf_component_register(SRCS "user_console.cpp"
                    REQUIRES driver nvs_flash console fatfs
                    cmd_nvs cmd_system
                    INCLUDE_DIRS "include")
