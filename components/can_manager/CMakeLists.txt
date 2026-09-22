idf_component_register(SRCS "can_manager.c"
                        INCLUDE_DIRS "include"
                        REQUIRES driver esp_timer lvgl__lvgl waveshare__esp32_p4_wifi6_touch_lcd_7b shift_lights)

# DIAGNOSTIC BUILDS - both are off by default. They log from inside the CAN RX
# task, and ESP_LOG blocks on the UART, so leaving them on at 500 kbit/s stalls
# the receive loop and makes the dash lag or drop frames. Turn one on only
# while hunting a mapping, then comment it out again.
#
# CAN_LOG_SCAN: dumps every CAN ID seen on the bus once per second, with the
# latest payload and both 16-bit decodings of each byte pair.
#target_compile_definitions(${COMPONENT_LIB} PRIVATE CAN_LOG_SCAN)
#
# CAN_LOG_DIFF: logs every byte that changes value, so shifting through the
# gears reveals the gear byte. Needs CAN_LOG_SCAN. Watch a single ID by adding
# e.g. CAN_LOG_DIFF_ID=0x121.
#target_compile_definitions(${COMPONENT_LIB} PRIVATE CAN_LOG_DIFF)
#
# CAN_LOG_GEARHUNT: lists every ID/byte on the bus that has ever changed
# value, with its distinct values and transition count. Shift N->1->N->1 and
# the gear byte is the one with ~one transition per shift.
#target_compile_definitions(${COMPONENT_LIB} PRIVATE CAN_LOG_GEARHUNT)
