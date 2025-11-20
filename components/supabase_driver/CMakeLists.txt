idf_component_register(SRCS "supabase_driver.cpp"
                      INCLUDE_DIRS "include"
                      REQUIRES nvs_flash esp_http_client esp-tls mbedtls Storage ErrorCodes
                      EMBED_TXTFILES "certs/supabase_root_ca.pem")

