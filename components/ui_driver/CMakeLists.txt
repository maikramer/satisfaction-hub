idf_component_register(SRCS "ui_driver.cpp" "ui_common.cpp" "roboto.c" "screens/wifi_config_screen.cpp" "screens/input_screen.cpp" "screens/wifi_scan_screen.cpp" "screens/brightness_screen.cpp" "screens/password_screen.cpp" "screens/ota_screen.cpp" "screens/about_screen.cpp"
                      INCLUDE_DIRS "include"
                      REQUIRES lvgl display_driver Wifi supabase_driver Storage ErrorCodes)

# Remover flags C++ do arquivo C e definir como C puro
set_source_files_properties(roboto.c PROPERTIES 
    COMPILE_FLAGS "-std=gnu11 -Wno-error"
    LANGUAGE C)
