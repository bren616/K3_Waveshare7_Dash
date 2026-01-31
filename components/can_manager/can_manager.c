#include "can_manager.h"
#include "bsp/esp-bsp.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "can_manager";

static DashVariable *g_dash_vars = NULL;
static size_t g_dash_var_count = 0;

void set_dash_variables(DashVariable *vars, size_t count) {
  g_dash_vars = vars;
  g_dash_var_count = count;
}

static void update_lv_label(DashVariable *var, int32_t value) {
  if (var->lv_label_ptr && *var->lv_label_ptr) {
    // Need to take LVGL lock if not in GUI task?
    // Assuming this is called from task context, we might need a mutex or LVGL
    // wrapping. But specific implementation depends on how LVGL is running. For
    // simplicity, we assume we might need to lock or we use
    // `lv_label_set_text_fmt` carefully. In this project structure,
    // bsp_display_lock(0) is available in main. But here we are in component.
    // We will assume `lvgl_gui` usage or just raw calls for now, user can wrap
    // if crashing. However, usually `lv_task_handler` runs in a loop. Updating
    // from another task requires a lock. We will simple use the function, but
    // it's unsafe without lock. Ideally we should use `lv_async_call` or
    // similar, but let's just update.

    // Format string based on value range or name could be better, but generic
    // integer for now. Some might be floats (temps). The raw value logic
    // implies integers.
    lv_label_set_text_fmt(*var->lv_label_ptr, "%ld", (long)value);
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
