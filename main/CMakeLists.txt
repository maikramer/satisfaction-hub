idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES display_driver ui_driver nvs_flash)