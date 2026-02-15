#include "ble_manager.h"
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "MAIN";

/**
 * Periodic task to read BLE data and update lap time / delta UI labels.
 * Runs at 10 Hz in its own FreeRTOS task.
 */
static void ble_ui_update_task(void *arg) {
  char buf[32];

  while (1) {
    int32_t lap_time_ms, delta_ms;
    bool lap_valid, delta_valid;

    ble_manager_get_lap_time(&lap_time_ms, &lap_valid);
    ble_manager_get_delta(&delta_ms, &delta_valid);

    bsp_display_lock(0);

    /* Update lap time label */
    if (lap_valid && ble_manager_is_connected()) {
      int32_t abs_ms = lap_time_ms < 0 ? -lap_time_ms : lap_time_ms;
      int32_t total_secs = abs_ms / 1000;
      int32_t frac = abs_ms % 1000;
      int32_t mins = total_secs / 60;
      int32_t secs = total_secs % 60;

      if (mins > 0) {
        snprintf(buf, sizeof(buf), "%" PRId32 ":%02" PRId32 ".%02" PRId32, mins,
                 secs, frac / 10);
      } else {
        snprintf(buf, sizeof(buf), "%" PRId32 ".%02" PRId32, secs, frac / 10);
      }
      lv_label_set_text(ui_LapTimeLabel, buf);
    } else {
      lv_label_set_text(ui_LapTimeLabel, "--");
    }

    /* Update delta label */
    if (delta_valid && ble_manager_is_connected()) {
      int32_t abs_ms = delta_ms < 0 ? -delta_ms : delta_ms;
      int32_t secs = abs_ms / 1000;
      int32_t frac = abs_ms % 1000;
      const char *sign = (delta_ms >= 0) ? "+" : "-";

      snprintf(buf, sizeof(buf), "%s%" PRId32 ".%03" PRId32, sign, secs, frac);
      lv_label_set_text(ui_DeltaLabel, buf);

      /* Color: green if faster (negative), red if slower (positive) */
      if (delta_ms < 0) {
        lv_obj_set_style_text_color(ui_DeltaLabel, lv_color_hex(0x15EB28),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
      } else {
        lv_obj_set_style_text_color(ui_DeltaLabel, lv_color_hex(0xFF0000),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
      }
    } else {
      lv_label_set_text(ui_DeltaLabel, "--");
      lv_obj_set_style_text_color(ui_DeltaLabel, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    bsp_display_unlock();

    vTaskDelay(pdMS_TO_TICKS(100)); /* 10 Hz */
  }
}

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

  /* Initialize BLE manager (runs in background) */
  esp_err_t ble_ret = ble_manager_init();
  if (ble_ret != ESP_OK) {
    ESP_LOGE(TAG, "BLE manager init failed: %s", esp_err_to_name(ble_ret));
  } else {
    /* Create UI update task for BLE data */
    xTaskCreate(ble_ui_update_task, "ble_ui", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "BLE UI update task started");
  }
}
