#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "can_manager.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lv_demos.h"
#include "lvgl.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ui.h"

void app_main(void) {
  bsp_display_cfg_t cfg = {.lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
                           .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
                           .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
                           .flags = {
                               .buff_dma = true,
                               .buff_spiram = false,
                               .sw_rotate = true,
                           }};
  lv_display_t *disp = bsp_display_start_with_config(&cfg);

  bsp_display_backlight_on();

  if (disp != NULL) {
    bsp_display_rotate(disp, LV_DISPLAY_ROTATION_180);
  }

  bsp_display_lock(0);

  ui_init();

  // Define CAN mapping
  static DashVariable dash_vars[] = {
      {"RPM", &ui_RPMLabel, 0x100, 2, 1, 0, 1000, 1000, 14000, 100, true},
      {"Gear", &ui_Gearlabel, 0x121, 1, -1, 2, 0, 0, 6, 1, true},
      {"WaterTemp", &ui_WaterTempLabel, 0x111, 2, 0, 1, 60, 60, 120, 1, true},
      {"OilTemp", &ui_OilTempLabel, 0x132, 2, 0, 1, 20, 20, 110, 1, true},
      {"OilPress", &ui_OilPressLabel, 0x133, 2, 0, 1, 1, 1, 90, 1, true},
      {"FLTemp", &ui_FLTempLabel, 0x126, 1, -1, 2, 5, 5, 35, 1, true},
      {"FRTemp", &ui_FRTempLabel, 0x127, 1, -1, 2, 5, 5, 35, 1, true},
      {"FLPress", &ui_FLPressLabel, 0x126, 2, 3, 4, 11, 11, 22, 1, true},
      {"FRPress", &ui_FRPressLabel, 0x127, 2, 3, 4, 11, 11, 22, 1, true},
      {"RLTemp", &ui_RLTempLabel, 0x128, 1, -1, 2, 5, 5, 35, 1, true},
      {"RRTemp", &ui_RRTempLabel, 0x129, 1, -1, 2, 5, 5, 35, 1, true},
      {"RLPress", &ui_RLPressLabel, 0x128, 2, 3, 4, 11, 11, 22, 1, true},
      {"RRPress", &ui_RRPressLabel, 0x129, 2, 3, 4, 11, 11, 22, 1, true}};

  init_can_manager();
  set_dash_variables(dash_vars, sizeof(dash_vars) / sizeof(dash_vars[0]));

  bsp_display_unlock();
}
