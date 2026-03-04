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

static lv_obj_t *ui_DeltaSpeedBarContainer = NULL;
static lv_obj_t *ui_DeltaSpeedBarFill = NULL;
static lv_obj_t *ui_DeltaSpeedBarLabel = NULL;

static void init_delta_speed_bar(void) {
  /* Hide the old LapTime text label */
  lv_obj_add_flag(ui_LapTimeLabel, LV_OBJ_FLAG_HIDDEN);

  ui_DeltaSpeedBarContainer = lv_obj_create(ui_Screen1);
  lv_obj_set_size(ui_DeltaSpeedBarContainer, 334, 100);
  lv_obj_set_x(ui_DeltaSpeedBarContainer, 335);
  lv_obj_set_y(ui_DeltaSpeedBarContainer, -238);
  lv_obj_set_align(ui_DeltaSpeedBarContainer, LV_ALIGN_CENTER);

  lv_obj_set_style_bg_color(ui_DeltaSpeedBarContainer, lv_color_hex(0x312F2F),
                            0);
  lv_obj_set_style_border_color(ui_DeltaSpeedBarContainer,
                                lv_color_hex(0xF1F910), 0);
  lv_obj_set_style_border_width(ui_DeltaSpeedBarContainer, 2, 0);
  lv_obj_set_style_radius(ui_DeltaSpeedBarContainer, 10, 0);
  lv_obj_clear_flag(ui_DeltaSpeedBarContainer, LV_OBJ_FLAG_SCROLLABLE);

  ui_DeltaSpeedBarFill = lv_obj_create(ui_DeltaSpeedBarContainer);
  lv_obj_set_style_border_width(ui_DeltaSpeedBarFill, 0, 0);
  lv_obj_set_style_radius(ui_DeltaSpeedBarFill, 0, 0);
  lv_obj_set_size(ui_DeltaSpeedBarFill, 0, 96);
  lv_obj_clear_flag(ui_DeltaSpeedBarFill, LV_OBJ_FLAG_SCROLLABLE);

  int tick_x[] = {7,   21,  36,  51,  65,  80,  94,  109, 123, 138, 153, 167,
                  182, 197, 212, 227, 242, 257, 272, 287, 302, 317, 328};
  for (int i = 0; i < 23; i++) {
    lv_obj_t *tick = lv_obj_create(ui_DeltaSpeedBarContainer);
    lv_obj_set_style_bg_color(tick, lv_color_hex(0xF1F910), 0);
    lv_obj_set_style_border_width(tick, 0, 0);
    lv_obj_set_style_radius(tick, 0, 0);
    lv_obj_clear_flag(tick, LV_OBJ_FLAG_SCROLLABLE);

    int height = (i == 11) ? 34 : 28;
    int width = (i == 11) ? 2 : 1;
    lv_obj_set_size(tick, width, height);
    lv_obj_set_pos(tick, tick_x[i] - 2, 96 - height);
  }

  ui_DeltaSpeedBarLabel = lv_label_create(ui_DeltaSpeedBarContainer);
  lv_obj_set_align(ui_DeltaSpeedBarLabel, LV_ALIGN_CENTER);
  lv_obj_set_style_text_color(ui_DeltaSpeedBarLabel, lv_color_hex(0xF1F910), 0);
  lv_obj_set_style_text_font(ui_DeltaSpeedBarLabel, &ui_font_timefont, 0);
  lv_label_set_text(ui_DeltaSpeedBarLabel, "0");
}

/**
 * Periodic task to read BLE data and update lap time / delta UI labels.
 * Runs at 10 Hz in its own FreeRTOS task.
 */
static void ble_ui_update_task(void *arg) {
  char buf[32];

  while (1) {
    int32_t delta_speed_val, delta_time_ms;
    bool speed_valid, time_valid;

    ble_manager_get_delta_speed(&delta_speed_val, &speed_valid);
    ble_manager_get_delta(&delta_time_ms, &time_valid);

    bsp_display_lock(0);

    /* Update speed bar graph */
    if (speed_valid && ble_manager_is_connected()) {
      int32_t abs_val =
          delta_speed_val < 0 ? -delta_speed_val : delta_speed_val;
      int32_t int_part = abs_val / 100;

      if (int_part == 0) {
        snprintf(buf, sizeof(buf), "0");
      } else {
        const char *sign = (delta_speed_val > 0) ? "+" : "-";
        snprintf(buf, sizeof(buf), "%s%" PRId32, sign, int_part);
      }
      lv_label_set_text(ui_DeltaSpeedBarLabel, buf);

      /* Bar sweep logic */
      float fraction = (float)abs_val / 1100.0f; /* 11 km/h * 100 */
      if (fraction > 1.0f)
        fraction = 1.0f;
      int bar_width = (int)(fraction * 165.0f);

      lv_obj_set_size(ui_DeltaSpeedBarFill, bar_width, 96);
      if (delta_speed_val > 0) { /* faster -> green fill left */
        lv_obj_set_style_bg_color(ui_DeltaSpeedBarFill, lv_color_hex(0x03A208),
                                  0);
        lv_obj_set_pos(ui_DeltaSpeedBarFill, 165 - bar_width, 0);
      } else if (delta_speed_val < 0) { /* slower -> red fill right */
        lv_obj_set_style_bg_color(ui_DeltaSpeedBarFill, lv_color_hex(0xFF0000),
                                  0);
        lv_obj_set_pos(ui_DeltaSpeedBarFill, 165, 0);
      } else {
        lv_obj_set_size(ui_DeltaSpeedBarFill, 0, 96);
      }
    } else {
      lv_label_set_text(ui_DeltaSpeedBarLabel, "--");
      lv_obj_set_size(ui_DeltaSpeedBarFill, 0, 96);
    }

    /* Update delta time label */
    if (time_valid && ble_manager_is_connected()) {
      int32_t abs_ms = delta_time_ms < 0 ? -delta_time_ms : delta_time_ms;
      int32_t secs = abs_ms / 1000;
      int32_t frac = abs_ms % 1000;
      const char *sign = (delta_time_ms >= 0) ? "+" : "-";

      snprintf(buf, sizeof(buf), "%s%" PRId32 ".%03" PRId32, sign, secs, frac);
      lv_label_set_text(ui_DeltaLabel, buf);

      /* Color: green if faster (negative time delta), red if slower (positive)
       */
      if (delta_time_ms < 0) {
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

  init_delta_speed_bar();

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
