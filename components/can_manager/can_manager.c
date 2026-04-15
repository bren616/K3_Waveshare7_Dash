#include "can_manager.h"
#include "bsp/esp-bsp.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "shift_lights.h"

static const char *TAG = "can_manager";

static DashVariable *g_dash_vars = NULL;
static size_t g_dash_var_count = 0;

void set_dash_variables(DashVariable *vars, size_t count) {
  g_dash_vars = vars;
  g_dash_var_count = count;
}

#include <string.h>

extern lv_obj_t * ui_Screen2;
extern lv_obj_t * ui_Screen2_RPMLabel;
extern lv_obj_t * ui_Screen2_RPMBar;
extern lv_obj_t * ui_Screen2_GearLabel;
extern lv_obj_t * ui_Screen2_WaterTempLabel;
extern lv_obj_t * ui_Screen2_WaterTempBar;
extern lv_obj_t * ui_Screen2_OilTempLabel;
extern lv_obj_t * ui_Screen2_OilTempBar;
extern lv_obj_t * ui_Screen2_OilPressLabel;
extern lv_obj_t * ui_Screen2_OilPressBar;
extern lv_obj_t * ui_Screen2_FLTempLabel;
extern lv_obj_t * ui_Screen2_FRTempLabel;
extern lv_obj_t * ui_Screen2_FLPressLabel;
extern lv_obj_t * ui_Screen2_FRPressLabel;
extern lv_obj_t * ui_Screen2_RLTempLabel;
extern lv_obj_t * ui_Screen2_RRTempLabel;
extern lv_obj_t * ui_Screen2_RLPressLabel;
extern lv_obj_t * ui_Screen2_RRPressLabel;

void update_lv_label(DashVariable *var, int32_t value) {
  if (var->lv_label_ptr && *var->lv_label_ptr) {
    lv_label_set_text_fmt(*var->lv_label_ptr, "%ld", (long)value);
  }

  // Also update ui_Screen2 elements if they are initialized
  if (!ui_Screen2) return;

  if (strcmp(var->name, "RPM") == 0) {
      if(ui_Screen2_RPMLabel) lv_label_set_text_fmt(ui_Screen2_RPMLabel, "%ld", (long)value);
      if(ui_Screen2_RPMBar) lv_bar_set_value(ui_Screen2_RPMBar, value, LV_ANIM_OFF);
  } else if (strcmp(var->name, "Gear") == 0) {
      if(ui_Screen2_GearLabel) {
          if (value == 0) lv_label_set_text(ui_Screen2_GearLabel, "N");
          else lv_label_set_text_fmt(ui_Screen2_GearLabel, "%ld", (long)value);
      }
  } else if (strcmp(var->name, "WaterTemp") == 0) {
      if(ui_Screen2_WaterTempLabel) lv_label_set_text_fmt(ui_Screen2_WaterTempLabel, "%ld", (long)value);
      if(ui_Screen2_WaterTempBar) lv_bar_set_value(ui_Screen2_WaterTempBar, value, LV_ANIM_OFF);
  } else if (strcmp(var->name, "OilTemp") == 0) {
      if(ui_Screen2_OilTempLabel) lv_label_set_text_fmt(ui_Screen2_OilTempLabel, "%ld", (long)value);
      if(ui_Screen2_OilTempBar) lv_bar_set_value(ui_Screen2_OilTempBar, value, LV_ANIM_OFF);
  } else if (strcmp(var->name, "OilPress") == 0) {
      if(ui_Screen2_OilPressLabel) lv_label_set_text_fmt(ui_Screen2_OilPressLabel, "%ld", (long)value);
      if(ui_Screen2_OilPressBar) lv_bar_set_value(ui_Screen2_OilPressBar, value, LV_ANIM_OFF);
  } else if (strcmp(var->name, "FLTemp") == 0) {
      if(ui_Screen2_FLTempLabel) lv_label_set_text_fmt(ui_Screen2_FLTempLabel, "%ld °C", (long)value);
  } else if (strcmp(var->name, "FRTemp") == 0) {
      if(ui_Screen2_FRTempLabel) lv_label_set_text_fmt(ui_Screen2_FRTempLabel, "%ld °C", (long)value);
  } else if (strcmp(var->name, "FLPress") == 0) {
      if(ui_Screen2_FLPressLabel) lv_label_set_text_fmt(ui_Screen2_FLPressLabel, "%ld PSI", (long)value);
  } else if (strcmp(var->name, "FRPress") == 0) {
      if(ui_Screen2_FRPressLabel) lv_label_set_text_fmt(ui_Screen2_FRPressLabel, "%ld PSI", (long)value);
  } else if (strcmp(var->name, "RLTemp") == 0) {
      if(ui_Screen2_RLTempLabel) lv_label_set_text_fmt(ui_Screen2_RLTempLabel, "%ld °C", (long)value);
  } else if (strcmp(var->name, "RRTemp") == 0) {
      if(ui_Screen2_RRTempLabel) lv_label_set_text_fmt(ui_Screen2_RRTempLabel, "%ld °C", (long)value);
  } else if (strcmp(var->name, "RLPress") == 0) {
      if(ui_Screen2_RLPressLabel) lv_label_set_text_fmt(ui_Screen2_RLPressLabel, "%ld PSI", (long)value);
  } else if (strcmp(var->name, "RRPress") == 0) {
      if(ui_Screen2_RRPressLabel) lv_label_set_text_fmt(ui_Screen2_RRPressLabel, "%ld PSI", (long)value);
  }
}

static void can_rx_task(void *arg) {
  twai_message_t message;
  while (1) {
    if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
      if (g_dash_vars) {
        for (size_t i = 0; i < g_dash_var_count; i++) {
          DashVariable *var = &g_dash_vars[i];
          if (message.identifier == var->can_id) {
            int32_t raw_val = 0;

            // Parse based on bytes
            if (var->num_bytes == 1) {
              if (var->lsb_idx < message.data_length_code) {
                raw_val = message.data[var->lsb_idx];
              }
            } else if (var->num_bytes == 2) {
              if (var->msb_idx < message.data_length_code &&
                  var->lsb_idx < message.data_length_code) {
                raw_val = (message.data[var->msb_idx] << 8) |
                          message.data[var->lsb_idx];
              }
            }

            var->current_val =
                raw_val; // Direct mapping for now, no scaling applied yet
            bsp_display_lock(0);
            update_lv_label(var, var->current_val);
            bsp_display_unlock();

            /* Drive shift lights from RPM (CAN ID 0x100) */
            if (message.identifier == 0x100) {
              shift_lights_update(raw_val);
            }
          }
        }
      }
    }
  }
}

void init_can_manager(void) {
  // Initialize TWAI
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_22, GPIO_NUM_21, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    ESP_LOGI(TAG, "Driver installed");
  } else {
    ESP_LOGE(TAG, "Failed to install driver");
    return;
  }

  // Start TWAI driver
  if (twai_start() == ESP_OK) {
    ESP_LOGI(TAG, "Driver started");
  } else {
    ESP_LOGE(TAG, "Failed to start driver");
    return;
  }

  // Create tasks
  // Mock data task removed for real data usage

  // Also create RX task (it won't receive anything if not connected, but good
  // to have ready)
  xTaskCreate(can_rx_task, "can_rx", 4096, NULL, 5, NULL);
}
