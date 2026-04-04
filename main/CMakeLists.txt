file(GLOB_RECURSE UI_SOURCES "ui/*.c")
idf_component_register(    
    SRCS 
        main.c
        ${UI_SOURCES}
    INCLUDE_DIRS ui .
    REQUIRES can_manager ble_manager shift_lights waveshare__esp32_p4_wifi6_touch_lcd_7b bsp_extra nvs_flash esp_hosted)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE 
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -DLV_USE_DEMO_MUSIC
)

# Uncomment the next line to enable RPM shift-light test sweep (no CAN needed)
target_compile_definitions(${COMPONENT_LIB} PRIVATE SHIFT_LIGHT_TEST_MODE)
