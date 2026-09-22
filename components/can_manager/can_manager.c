#include "can_manager.h"
#include <stdio.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "esp_timer.h"
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
  /* Gear 0 means neutral on both screens - show "N" rather than "0".
   * NOTE: ui_font_Font2 must contain the 'N' glyph or LVGL draws a
   * placeholder box instead (see the --symbols list used to build it). */
  bool is_gear = (strcmp(var->name, "Gear") == 0);

  if (var->lv_label_ptr && *var->lv_label_ptr) {
    if (is_gear && value == 0) {
      lv_label_set_text(*var->lv_label_ptr, "N");
    } else {
      lv_label_set_text_fmt(*var->lv_label_ptr, "%ld", (long)value);
    }
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

/* Define CAN_LOG_RAW to dump raw frame bytes for CAN_LOG_RAW_ID (default
 * 0x100 / RPM) at ~1 Hz. Use this to confirm the byte order and scaling the
 * ECU actually sends before trusting the decoded value. */
#ifndef CAN_LOG_RAW_ID
#define CAN_LOG_RAW_ID 0x100
#endif

/* Define CAN_LOG_SCAN to dump a table of every distinct CAN ID seen on the
 * bus once per second, with its latest payload and both 16-bit decodings of
 * each byte pair. Use it to find which ID/byte offset actually carries a
 * channel when a value reads as a constant 0. */
#ifdef CAN_LOG_SCAN
#define SCAN_MAX_IDS 40
typedef struct {
  uint32_t id;
  uint8_t dlc;
  uint8_t data[8];
  uint32_t count;
} can_scan_entry_t;

static can_scan_entry_t g_scan[SCAN_MAX_IDS];
static size_t g_scan_n = 0;

static void can_scan_record(const twai_message_t *m) {
  size_t i;
  for (i = 0; i < g_scan_n; i++) {
    if (g_scan[i].id == m->identifier) break;
  }
  if (i == g_scan_n) {
    if (g_scan_n >= SCAN_MAX_IDS) return;
    g_scan_n++;
    g_scan[i].id = m->identifier;
    g_scan[i].count = 0;
  }
#ifdef CAN_LOG_DIFF
  /* Log every byte that actually changes value. Shift through the gears and
   * whichever byte tracks the lever is the gear byte - on whatever ID it
   * turns out to live. Define CAN_LOG_DIFF_ID to watch a single ID. */
  if (g_scan[i].count > 0) {
    for (int b = 0; b < m->data_length_code && b < 8; b++) {
      if (g_scan[i].data[b] != m->data[b]) {
#ifdef CAN_LOG_DIFF_ID
        if (m->identifier != CAN_LOG_DIFF_ID) continue;
#endif
        ESP_LOGW(TAG, "CHANGE 0x%03lX byte[%d]: 0x%02X -> 0x%02X  (%u -> %u)",
                 (unsigned long)m->identifier, b, g_scan[i].data[b], m->data[b],
                 g_scan[i].data[b], m->data[b]);
      }
    }
  }
#endif

  g_scan[i].dlc = m->data_length_code;
  memcpy(g_scan[i].data, m->data, 8);
  g_scan[i].count++;
}

static void can_scan_dump(void) {
  ESP_LOGI(TAG, "===== bus scan: %u distinct IDs =====", (unsigned)g_scan_n);
  for (size_t i = 0; i < g_scan_n; i++) {
    const uint8_t *d = g_scan[i].data;
    ESP_LOGI(TAG,
             "  0x%03lX dlc=%u n=%lu [%02X %02X %02X %02X %02X %02X %02X %02X]"
             "  d0d1: BE=%u LE=%u   d2d3: BE=%u LE=%u",
             (unsigned long)g_scan[i].id, g_scan[i].dlc,
             (unsigned long)g_scan[i].count, d[0], d[1], d[2], d[3], d[4],
             d[5], d[6], d[7], (unsigned)((d[0] << 8) | d[1]),
             (unsigned)((d[1] << 8) | d[0]), (unsigned)((d[2] << 8) | d[3]),
             (unsigned)((d[3] << 8) | d[2]));
  }
}
#endif /* CAN_LOG_SCAN */

/* Define CAN_LOG_GEARHUNT to find which byte on the bus carries the gear
 * lever. Every 3 s it lists ONLY the ID/byte pairs whose value has ever
 * changed, with the set of distinct values seen and how many transitions.
 *
 * How to read it: shift N -> 1 -> N -> 1 a few times, then look for a byte
 * with a handful of transitions and two distinct values. A sensor byte shows
 * hundreds of transitions; the gear lever shows one per shift. Bytes that
 * never move are omitted entirely, which is itself the answer if NOTHING
 * moves when you shift - the gear is then not on this bus. */
#ifdef CAN_LOG_GEARHUNT
#define HUNT_MAX_IDS 40
#define HUNT_MAX_VALS 8
/* A byte that moves more than this many times is a sensor, not a lever. */
#define HUNT_NOISY 25

typedef struct {
  uint32_t id;
  uint8_t dlc;
  uint8_t vals[8][HUNT_MAX_VALS]; /* distinct values seen, per byte */
  uint8_t n_vals[8];
  bool overflow[8]; /* more than HUNT_MAX_VALS distinct values */
  uint32_t changes[8];
  uint8_t last[8];
  bool have_last;
} can_hunt_entry_t;

static can_hunt_entry_t g_hunt[HUNT_MAX_IDS];
static size_t g_hunt_n = 0;

static void can_hunt_record(const twai_message_t *m) {
  size_t i;
  for (i = 0; i < g_hunt_n; i++) {
    if (g_hunt[i].id == m->identifier) break;
  }
  if (i == g_hunt_n) {
    if (g_hunt_n >= HUNT_MAX_IDS) return;
    g_hunt_n++;
    memset(&g_hunt[i], 0, sizeof(g_hunt[i]));
    g_hunt[i].id = m->identifier;
  }

  can_hunt_entry_t *e = &g_hunt[i];
  e->dlc = m->data_length_code;
  for (int b = 0; b < m->data_length_code && b < 8; b++) {
    uint8_t v = m->data[b];
    if (e->have_last && e->last[b] != v) {
      e->changes[b]++;
      /* Print the transition the instant it happens, so a shift shows up in
       * the monitor while your hand is still on the lever. A byte that has
       * moved more than HUNT_NOISY times is a sensor, not a lever, so it is
       * suppressed from then on and the log stays readable. */
      if (e->changes[b] <= HUNT_NOISY) {
        ESP_LOGW(TAG, "MOVED 0x%03lX byte[%d]: %02X -> %02X   (change #%lu)",
                 (unsigned long)m->identifier, b, e->last[b], v,
                 (unsigned long)e->changes[b]);
      } else if (e->changes[b] == HUNT_NOISY + 1) {
        ESP_LOGW(TAG, "0x%03lX byte[%d] has moved %d times - treating it as a "
                      "sensor, suppressing further lines for it",
                 (unsigned long)m->identifier, b, HUNT_NOISY);
      }
    }
    bool known = false;
    for (uint8_t k = 0; k < e->n_vals[b]; k++) {
      if (e->vals[b][k] == v) {
        known = true;
        break;
      }
    }
    if (!known) {
      if (e->n_vals[b] < HUNT_MAX_VALS) e->vals[b][e->n_vals[b]++] = v;
      else e->overflow[b] = true;
    }
    e->last[b] = v;
  }
  e->have_last = true;
}

static void can_hunt_dump(void) {
  bool any = false;
  ESP_LOGI(TAG, "===== bytes that have MOVED (%u IDs on bus) =====",
           (unsigned)g_hunt_n);
  for (size_t i = 0; i < g_hunt_n; i++) {
    can_hunt_entry_t *e = &g_hunt[i];
    for (int b = 0; b < e->dlc && b < 8; b++) {
      if (e->n_vals[b] < 2) continue; /* constant - cannot be the lever */
      any = true;
      char buf[48];
      int o = 0;
      for (uint8_t k = 0; k < e->n_vals[b] && o < (int)sizeof(buf) - 4; k++) {
        o += snprintf(buf + o, sizeof(buf) - o, "%02X ", e->vals[b][k]);
      }
      ESP_LOGI(TAG, "  0x%03lX byte[%d] dlc=%u changes=%lu distinct=%u%s : %s",
               (unsigned long)e->id, b, e->dlc, (unsigned long)e->changes[b],
               e->n_vals[b], e->overflow[b] ? "+" : "", buf);
    }
  }
  if (!any) {
    ESP_LOGW(TAG, "  nothing on the bus has changed value yet");
  }
}
#endif /* CAN_LOG_GEARHUNT */

static void can_rx_task(void *arg) {
  twai_message_t message;
  while (1) {
    if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
#ifdef CAN_LOG_GEARHUNT
      can_hunt_record(&message);
      {
        static int64_t last_hunt_us = 0;
        int64_t now_us = esp_timer_get_time();
        if (now_us - last_hunt_us > 3000000) {
          last_hunt_us = now_us;
          can_hunt_dump();
        }
      }
#endif
#ifdef CAN_LOG_SCAN
      can_scan_record(&message);
      {
        static int64_t last_dump_us = 0;
        int64_t now_us = esp_timer_get_time();
        if (now_us - last_dump_us > 1000000) {
          last_dump_us = now_us;
          can_scan_dump();
        }
      }
#endif
#ifdef CAN_LOG_RAW
      if (message.identifier == CAN_LOG_RAW_ID) {
        static int64_t last_log_us = 0;
        int64_t now_us = esp_timer_get_time();
        if (now_us - last_log_us > 1000000) {
          last_log_us = now_us;
          ESP_LOGI(TAG,
                   "raw 0x%03lX dlc=%d [%02X %02X %02X %02X %02X %02X %02X "
                   "%02X]  BE(d0d1)=%u  LE(d1d0)=%u",
                   (unsigned long)message.identifier,
                   message.data_length_code, message.data[0], message.data[1],
                   message.data[2], message.data[3], message.data[4],
                   message.data[5], message.data[6], message.data[7],
                   (unsigned)((message.data[0] << 8) | message.data[1]),
                   (unsigned)((message.data[1] << 8) | message.data[0]));
        }
      }
#endif
      if (g_dash_vars) {
        for (size_t i = 0; i < g_dash_var_count; i++) {
          DashVariable *var = &g_dash_vars[i];
          if (message.identifier == var->can_id) {
            int32_t raw_val = 0;
            bool decoded = false;

            // Parse based on bytes
            if (var->num_bytes == 1) {
              if (var->lsb_idx >= 0 && var->lsb_idx < message.data_length_code) {
                raw_val = message.data[var->lsb_idx];
                decoded = true;
              }
            } else if (var->num_bytes == 2) {
              if (var->msb_idx >= 0 && var->lsb_idx >= 0 &&
                  var->msb_idx < message.data_length_code &&
                  var->lsb_idx < message.data_length_code) {
                raw_val = (message.data[var->msb_idx] << 8) |
                          message.data[var->lsb_idx];
                decoded = true;
              }
            }

            /* Frame is too short (or the mapping is misconfigured) for the
             * byte offsets this variable wants. Leave the label showing its
             * previous value rather than writing a misleading 0. */
            if (!decoded) {
              /* Rate-limit per variable, not globally, so one short channel
               * cannot hide another. */
              static int64_t last_warn_us[32] = {0};
              int64_t now_us = esp_timer_get_time();
              size_t wi = (i < 32) ? i : 31;
              if (now_us - last_warn_us[wi] > 1000000) {
                last_warn_us[wi] = now_us;
                ESP_LOGW(TAG,
                         "%s: frame 0x%03lX has dlc=%d, too short for byte "
                         "idx msb=%d lsb=%d - skipping update",
                         var->name, (unsigned long)message.identifier,
                         message.data_length_code, var->msb_idx, var->lsb_idx);
              }
              continue;
            }

            /* Raw bus value -> engineering units. Identity unless the
             * variable declares a scale/offset. */
            {
              int32_t mul = var->scale_mul ? var->scale_mul : 1;
              int32_t div = var->scale_div ? var->scale_div : 1;
              raw_val = raw_val * mul / div + var->offset;
            }

            /* Gear comes straight off the bus as a small integer; anything
             * outside 0..8 is a byte we have mis-mapped or a transient, and
             * painting it would flash nonsense over a good reading. */
            if (strcmp(var->name, "Gear") == 0) {
              if (raw_val < 0 || raw_val > 8) continue;
#ifdef CAN_LOG_GEARHUNT
              {
                static int64_t last_gear_us = 0;
                int64_t now_us = esp_timer_get_time();
                if (now_us - last_gear_us > 1000000) {
                  last_gear_us = now_us;
                  ESP_LOGI(TAG, "Gear decoded from 0x%03lX byte[%d] = %ld",
                           (unsigned long)message.identifier, var->lsb_idx,
                           (long)raw_val);
                }
              }
#endif
            }

            /* Drive shift lights from RPM (CAN ID 0x100) - unconditional, it
             * is cheap and must not be gated on the value having changed. */
            if (message.identifier == 0x100) {
              shift_lights_update(raw_val);
            }

            /* These frames arrive at 40-90 Hz each. Taking the display lock
             * and re-laying out a label for every one of them starves the
             * LVGL task and leaves the screen visibly stale, so only touch
             * the UI when the value actually changed. */
            if (var->ui_valid && raw_val == var->current_val) {
              continue;
            }

            var->current_val = raw_val;
            var->ui_valid = true;
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
