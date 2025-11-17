idf_component_register(SRCS "ui_driver.cpp" "ui_common.cpp" "roboto.c" "screens/wifi_config_screen.cpp" "screens/input_screen.cpp" "screens/wifi_scan_screen.cpp"
                      INCLUDE_DIRS "include"
                      REQUIRES lvgl display_driver wifi_manager)

