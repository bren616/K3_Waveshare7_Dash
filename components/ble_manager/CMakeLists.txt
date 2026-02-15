idf_component_register(SRCS "ble_manager.c"
                        INCLUDE_DIRS "include"
                        REQUIRES bt nvs_flash espressif__esp_hosted)
