idf_component_register(SRCS "elrs.cpp"
                    REQUIRES driver
                    INCLUDE_DIRS "include")

set_source_files_properties("elrs.cpp"
                    PROPERTIES COMPILE_FLAGS
                     -Wno-error=switch)                   