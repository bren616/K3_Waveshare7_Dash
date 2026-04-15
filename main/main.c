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
#include "shift_lights.h"
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
      if (ui_Screen2_DeltaLabel) lv_label_set_text(ui_Screen2_DeltaLabel, buf);
      if (ui_Screen2_DeltaBar) lv_bar_set_value(ui_Screen2_DeltaBar, delta_time_ms / 10, LV_ANIM_OFF); // +/- 500 represents 5 secs

      /* Color: green if faster (negative time delta), red if slower (positive)
       */
      if (delta_time_ms < 0) {
        lv_obj_set_style_text_color(ui_DeltaLabel, lv_color_hex(0x15EB28),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_Screen2_DeltaLabel) lv_obj_set_style_text_color(ui_Screen2_DeltaLabel, lv_color_hex(0x15EB28), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_Screen2_DeltaBar) lv_obj_set_style_bg_color(ui_Screen2_DeltaBar, lv_color_hex(0x15EB28), LV_PART_INDICATOR | LV_STATE_DEFAULT);
      } else {
        lv_obj_set_style_text_color(ui_DeltaLabel, lv_color_hex(0xFF0000),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_Screen2_DeltaLabel) lv_obj_set_style_text_color(ui_Screen2_DeltaLabel, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (ui_Screen2_DeltaBar) lv_obj_set_style_bg_color(ui_Screen2_DeltaBar, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);
      }
    } else {
      lv_label_set_text(ui_DeltaLabel, "--");
      lv_obj_set_style_text_color(ui_DeltaLabel, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
      if (ui_Screen2_DeltaLabel) {
          lv_label_set_text(ui_Screen2_DeltaLabel, "--");
          lv_obj_set_style_text_color(ui_Screen2_DeltaLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
      }
      if (ui_Screen2_DeltaBar) {
          lv_bar_set_value(ui_Screen2_DeltaBar, 0, LV_ANIM_OFF);
      }
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

  /* Initialize shift lights (WS2812B LEDs) */
  esp_err_t sl_ret = shift_lights_init();
  if (sl_ret != ESP_OK) {
    ESP_LOGE(TAG, "Shift lights init failed: %s", esp_err_to_name(sl_ret));
  }

#ifdef SHIFT_LIGHT_TEST_MODE
  /* Launch the shift-light test sweep instead of waiting for real CAN data */
  extern void shift_light_test_task(void *arg);
  xTaskCreate(shift_light_test_task, "sl_test", 4096, NULL, 4, NULL);
  ESP_LOGW(TAG, "*** SHIFT LIGHT TEST MODE ACTIVE ***");
#endif

  /* Initialize BLE manager (runs in background) */
  esp_err_t ble_ret = ble_manager_init();
  if (ble_ret != ESP_OK) {
    ESP_LOGE(TAG, "BLE manager init failed: %s", esp_err_to_name(ble_ret));
  } else {
#ifndef DEMO_MODE
    /* Create UI update task for BLE data (skip in demo mode) */
    xTaskCreate(ble_ui_update_task, "ble_ui", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "BLE UI update task started");
#endif
  }

#ifdef DEMO_MODE
  extern void demo_mode_task(void *arg);
  /* Pass the dash_vars array to the demo task */
  typedef struct { DashVariable *vars; size_t count; } DemoArgs;
  static DemoArgs demo_args = { dash_vars, sizeof(dash_vars) / sizeof(dash_vars[0]) };
  xTaskCreate(demo_mode_task, "demo", 8192, &demo_args, 4, NULL);
  ESP_LOGW(TAG, "*** DEMO MODE ACTIVE ***");
#endif
}

/*
 * ===== SHIFT LIGHT TEST SWEEP =====
 *
 * Compile with -DSHIFT_LIGHT_TEST_MODE to enable.
 * Ramps RPM 0 → 14000 in steps of 250, pauses at each step so you
 * can verify each LED threshold. Holds at 14000 for 5 s to test the
 * Red/White flash mode, then ramps back down to 0 and repeats.
 */
#ifdef SHIFT_LIGHT_TEST_MODE
void shift_light_test_task(void *arg) {
  const int32_t rpm_max = 14000;
  const int32_t rpm_step = 250;
  const int step_delay_ms = 300;  /* time at each RPM step */
  const int flash_hold_ms = 5000; /* hold time above 13000 RPM */

  ESP_LOGI(TAG,
           "Shift light test: sweeping 0 -> %ld -> 0 (step=%ld, delay=%dms)",
           (long)rpm_max, (long)rpm_step, step_delay_ms);

  while (1) {
    /* Ramp UP */
    for (int32_t rpm = 0; rpm <= rpm_max; rpm += rpm_step) {
      ESP_LOGI(TAG, "TEST RPM: %ld", (long)rpm);
      shift_lights_update(rpm);
      vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
    }

    /* Hold in flash zone */
    ESP_LOGW(TAG, "TEST: Holding at %ld RPM (flash zone) for %d ms",
             (long)rpm_max, flash_hold_ms);
    TickType_t hold_end = xTaskGetTickCount() + pdMS_TO_TICKS(flash_hold_ms);
    while (xTaskGetTickCount() < hold_end) {
      shift_lights_update(rpm_max);
      vTaskDelay(pdMS_TO_TICKS(50)); /* fast updates to show flashing */
    }

    /* Ramp DOWN */
    for (int32_t rpm = rpm_max; rpm >= 0; rpm -= rpm_step) {
      ESP_LOGI(TAG, "TEST RPM: %ld", (long)rpm);
      shift_lights_update(rpm);
      vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
    }

    /* Pause before next cycle */
    ESP_LOGI(TAG, "TEST: Cycle complete. Restarting in 2s...");
    shift_lights_update(0);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
#endif

/*
 * ===== DEMO MODE =====
 *
 * Compile with -DDEMO_MODE to enable.
 *
 * Simulates a realistic race scenario:
 *  - RPM sweeps up through gears, drops on gear change
 *  - Gear cycles 1→6
 *  - Oil/Water temperatures warm up then settle with small fluctuations
 *  - Tire temps and pressures fluctuate realistically
 *  - Lap time counts up, delta oscillates green/red
 *  - Shift lights driven from RPM
 *
 * Updates both Screen1 (via DashVariable pipeline) and Screen2 at 20 Hz.
 */
#ifdef DEMO_MODE

#include <math.h>

/* Smoothly interpolate a value toward a target */
static int32_t approach(int32_t current, int32_t target, int32_t step) {
  if (current < target) {
    current += step;
    if (current > target) current = target;
  } else if (current > target) {
    current -= step;
    if (current < target) current = target;
  }
  return current;
}

/* Simple pseudo-random jitter (±range) around a center */
static int32_t jitter(int32_t center, int32_t range, uint32_t seed) {
  /* xorshift32 */
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  int32_t r = (int32_t)(seed % (2 * range + 1)) - range;
  return center + r;
}

void demo_mode_task(void *arg) {
  ESP_LOGI(TAG, "Demo mode: starting simulated race data");

  /* Receive dash_vars from caller */
  typedef struct { DashVariable *vars; size_t count; } DemoArgs;
  DemoArgs *dargs = (DemoArgs *)arg;
  DashVariable *dash_vars = dargs->vars;
  size_t dash_var_count = dargs->count;

  /* ── State variables ──────────────────────────── */
  int32_t rpm = 800;           /* start at idle */
  int32_t gear = 1;
  int32_t oil_temp = 20;       /* cold start */
  int32_t oil_press = 10;
  int32_t water_temp = 25;     /* cold start */
  int32_t fl_temp = 15, fr_temp = 15, rl_temp = 15, rr_temp = 15;
  int32_t fl_press = 26, fr_press = 25, rl_press = 26, rr_press = 26;

  /* Gear shift RPM thresholds */
  const int32_t shift_rpm = 13800;
  const int32_t drop_rpm  = 7000;
  const int32_t idle_rpm  = 800;
  bool rpm_rising = true;
  int32_t rpm_step = 120;

  /* Delta simulation */
  float delta_phase = 0.0f;    /* oscillates for demo effect */
  int32_t lap_time_ms = 0;     /* simulated lap timer */

  /* Warm-up targets */
  const int32_t oil_temp_hot  = 105;
  const int32_t water_temp_hot = 90;
  const int32_t oil_press_hot = 72;

  uint32_t tick = 0;
  uint32_t rng_seed = 12345;

  const int update_ms = 50; /* 20 Hz */

  while (1) {
    tick++;
    rng_seed = rng_seed * 1103515245 + 12345; /* LCG for jitter */

    /* ── RPM & Gear simulation ──────────────── */
    if (rpm_rising) {
      rpm += rpm_step;
      if (rpm >= shift_rpm) {
        /* Shift up */
        gear++;
        if (gear > 6) {
          gear = 6;
          rpm_rising = false; /* start coming back down */
        } else {
          rpm = drop_rpm; /* RPM drops on upshift */
        }
      }
    } else {
      rpm -= rpm_step;
      if (rpm <= idle_rpm + 500) {
        /* Downshift */
        gear--;
        if (gear < 1) {
          gear = 1;
          rpm_rising = true; /* start climbing again */
          rpm = idle_rpm;
        } else {
          rpm = shift_rpm - 1500; /* RPM jumps on downshift */
        }
      }
    }
    /* Add small RPM jitter for realism */
    int32_t rpm_display = rpm + (int32_t)((rng_seed >> 16) % 201) - 100;
    if (rpm_display < 0) rpm_display = 0;

    /* ── Temperatures warm up then settle ─────── */
    oil_temp  = approach(oil_temp,  jitter(oil_temp_hot,  3, rng_seed), 1);
    water_temp = approach(water_temp, jitter(water_temp_hot, 2, rng_seed >> 8), 1);
    oil_press = approach(oil_press, jitter(oil_press_hot, 5, rng_seed >> 4), 1);

    /* Tire temps slowly warm and fluctuate */
    int32_t tire_temp_target = 35 + (rpm > 9000 ? 8 : 0);
    fl_temp = approach(fl_temp, jitter(tire_temp_target,     2, rng_seed), 1);
    fr_temp = approach(fr_temp, jitter(tire_temp_target - 1, 2, rng_seed >> 3), 1);
    rl_temp = approach(rl_temp, jitter(tire_temp_target + 2, 3, rng_seed >> 5), 1);
    rr_temp = approach(rr_temp, jitter(tire_temp_target + 1, 2, rng_seed >> 7), 1);

    /* Tire pressures fluctuate slightly */
    fl_press = jitter(26, 1, rng_seed >> 2);
    fr_press = jitter(25, 1, rng_seed >> 6);
    rl_press = jitter(26, 1, rng_seed >> 9);
    rr_press = jitter(26, 1, rng_seed >> 11);

    /* ── Lap time & Delta simulation ──────────── */
    lap_time_ms += update_ms;
    if (lap_time_ms > 95000) lap_time_ms = 0; /* ~1:35 lap, then reset */

    delta_phase += 0.03f;
    if (delta_phase > 6.28f) delta_phase -= 6.28f;
    int32_t delta_time_ms = (int32_t)(sinf(delta_phase) * 2500.0f); /* ±2.5s */

    /* ── Push values through the DashVariable pipeline ── */
    /* This updates Screen1 labels + Screen2 via update_lv_label() */
    int32_t values[] = {
      rpm_display, gear, water_temp, oil_temp, oil_press,
      fl_temp, fr_temp, fl_press, fr_press,
      rl_temp, rr_temp, rl_press, rr_press
    };

    bsp_display_lock(0);

    for (size_t i = 0; i < dash_var_count && i < 13; i++) {
      dash_vars[i].current_val = values[i];
      update_lv_label(&dash_vars[i], values[i]);
    }

    /* ── Update delta / lap time on both screens ─── */
    char buf[32];

    /* Lap time: mm:ss.cc */
    int32_t lap_secs = lap_time_ms / 1000;
    int32_t lap_mins = lap_secs / 60;
    lap_secs %= 60;
    int32_t lap_hundredths = (lap_time_ms % 1000) / 10;
    snprintf(buf, sizeof(buf), "%" PRId32 ":%02" PRId32 ".%02" PRId32,
             lap_mins, lap_secs, lap_hundredths);

    if (ui_Screen2_LapTimeLabel)
      lv_label_set_text(ui_Screen2_LapTimeLabel, buf);

    /* Delta time */
    int32_t abs_delta = delta_time_ms < 0 ? -delta_time_ms : delta_time_ms;
    int32_t d_secs = abs_delta / 1000;
    int32_t d_frac = abs_delta % 1000;
    const char *d_sign = (delta_time_ms >= 0) ? "+" : "-";
    snprintf(buf, sizeof(buf), "%s%" PRId32 ".%03" PRId32, d_sign, d_secs, d_frac);

    /* Screen1 delta */
    lv_label_set_text(ui_DeltaLabel, buf);
    lv_color_t delta_col = (delta_time_ms < 0) ?
        lv_color_hex(0x15EB28) : lv_color_hex(0xFF0000);
    lv_obj_set_style_text_color(ui_DeltaLabel, delta_col,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Screen2 delta */
    if (ui_Screen2_DeltaLabel) {
      lv_label_set_text(ui_Screen2_DeltaLabel, buf);
      lv_obj_set_style_text_color(ui_Screen2_DeltaLabel, delta_col,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_Screen2_DeltaBar) {
      lv_bar_set_value(ui_Screen2_DeltaBar, delta_time_ms / 10, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(ui_Screen2_DeltaBar, delta_col,
                                LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }

    /* Screen1 speed bar (simulate delta speed from RPM) */
    int32_t delta_speed = (int32_t)(sinf(delta_phase * 1.7f) * 800.0f);
    int32_t abs_spd = delta_speed < 0 ? -delta_speed : delta_speed;
    int32_t spd_int = abs_spd / 100;
    if (spd_int == 0) {
      snprintf(buf, sizeof(buf), "0");
    } else {
      snprintf(buf, sizeof(buf), "%s%" PRId32,
               delta_speed > 0 ? "+" : "-", spd_int);
    }
    if (ui_DeltaSpeedBarLabel)
      lv_label_set_text(ui_DeltaSpeedBarLabel, buf);
    if (ui_DeltaSpeedBarFill) {
      float fraction = (float)abs_spd / 1100.0f;
      if (fraction > 1.0f) fraction = 1.0f;
      int bar_width = (int)(fraction * 165.0f);
      lv_obj_set_size(ui_DeltaSpeedBarFill, bar_width, 96);
      if (delta_speed > 0) {
        lv_obj_set_style_bg_color(ui_DeltaSpeedBarFill,
                                  lv_color_hex(0x03A208), 0);
        lv_obj_set_pos(ui_DeltaSpeedBarFill, 165 - bar_width, 0);
      } else if (delta_speed < 0) {
        lv_obj_set_style_bg_color(ui_DeltaSpeedBarFill,
                                  lv_color_hex(0xFF0000), 0);
        lv_obj_set_pos(ui_DeltaSpeedBarFill, 165, 0);
      } else {
        lv_obj_set_size(ui_DeltaSpeedBarFill, 0, 96);
      }
    }

    /* Best lap (static for demo) */
    if (ui_Screen2_BestLapLabel)
      lv_label_set_text(ui_Screen2_BestLapLabel, "1:33.80");

    bsp_display_unlock();

    /* Drive shift lights from RPM */
    shift_lights_update(rpm_display);

    vTaskDelay(pdMS_TO_TICKS(update_ms));
  }
}


#endif /* DEMO_MODE */


