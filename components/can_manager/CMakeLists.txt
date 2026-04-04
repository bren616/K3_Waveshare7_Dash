idf_component_register(SRCS "can_manager.c"
                        INCLUDE_DIRS "include"
                        REQUIRES driver esp_timer lvgl__lvgl waveshare__esp32_p4_wifi6_touch_lcd_7b shift_lights)
