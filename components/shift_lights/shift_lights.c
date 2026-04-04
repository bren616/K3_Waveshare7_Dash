#include "shift_lights.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include <string.h>

static const char *TAG = "SHIFT_LIGHTS";

/* ---------- Configuration ---------- */

#define SHIFT_LIGHT_GPIO 48
#define SHIFT_LIGHT_NUM_LEDS 10

/* RPM thresholds: 9 thresholds map to LEDs 0–8 (left to right).
 * LED 9 only participates in the >13000 RPM flash-all mode. */
#define RPM_THRESHOLD_START 9000
#define RPM_THRESHOLD_STEP 500
#define RPM_THRESHOLD_COUNT 9
#define RPM_FLASH_THRESHOLD 13000
#define FLASH_INTERVAL_MS 500

/* Colours (GRB order not needed — the led_strip API takes R, G, B) */
#define COLOR_BLUE_R 0
#define COLOR_BLUE_G 0
#define COLOR_BLUE_B 255

#define COLOR_ORANGE_R 255
#define COLOR_ORANGE_G 165
#define COLOR_ORANGE_B 0

#define COLOR_RED_R 255
#define COLOR_RED_G 0
#define COLOR_RED_B 0

#define COLOR_WHITE_R 255
#define COLOR_WHITE_G 255
#define COLOR_WHITE_B 255

/* ---------- State ---------- */

static led_strip_handle_t g_strip = NULL;
static bool g_flash_state = false;         /* toggles every 500 ms */
static int64_t g_last_flash_toggle_us = 0; /* esp_timer timestamp */

/* Per-LED colour lookup (index = LED position 0–8) */
static const uint8_t led_colors[][3] = {
    {COLOR_BLUE_R, COLOR_BLUE_G, COLOR_BLUE_B},       /* LED 0 */
    {COLOR_BLUE_R, COLOR_BLUE_G, COLOR_BLUE_B},       /* LED 1 */
    {COLOR_BLUE_R, COLOR_BLUE_G, COLOR_BLUE_B},       /* LED 2 */
    {COLOR_ORANGE_R, COLOR_ORANGE_G, COLOR_ORANGE_B}, /* LED 3 */
    {COLOR_ORANGE_R, COLOR_ORANGE_G, COLOR_ORANGE_B}, /* LED 4 */
    {COLOR_ORANGE_R, COLOR_ORANGE_G, COLOR_ORANGE_B}, /* LED 5 */
    {COLOR_RED_R, COLOR_RED_G, COLOR_RED_B},          /* LED 6 */
    {COLOR_RED_R, COLOR_RED_G, COLOR_RED_B},          /* LED 7 */
    {COLOR_RED_R, COLOR_RED_G, COLOR_RED_B},          /* LED 8 */
};

/* ---------- Public API ---------- */

esp_err_t shift_lights_init(void) {
  led_strip_config_t strip_config = {
      .strip_gpio_num = SHIFT_LIGHT_GPIO,
      .max_leds = SHIFT_LIGHT_NUM_LEDS,
      .led_model = LED_MODEL_WS2812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
      .flags.invert_out = false,
  };

  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10 * 1000 * 1000, /* 10 MHz → 100 ns resolution */
      .mem_block_symbols = 0,            /* let driver auto-detect */
      .flags.with_dma = true, /* DMA for reliable timing (ESP32-P4 supported) */
  };

  esp_err_t ret =
      led_strip_new_rmt_device(&strip_config, &rmt_config, &g_strip);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
    return ret;
  }

  /* All LEDs off on startup — clear + small delay for WS2812B reset */
  led_strip_clear(g_strip);
  vTaskDelay(pdMS_TO_TICKS(10));

  ESP_LOGI(TAG, "Shift lights initialised on GPIO %d (%d LEDs, DMA enabled)",
           SHIFT_LIGHT_GPIO, SHIFT_LIGHT_NUM_LEDS);
  return ESP_OK;
}

void shift_lights_update(int32_t rpm) {
  if (g_strip == NULL) {
    return;
  }

  /* --- Flash mode: > 13000 RPM --- */
  if (rpm > RPM_FLASH_THRESHOLD) {
    int64_t now_us = esp_timer_get_time();
    if (now_us - g_last_flash_toggle_us >= (int64_t)FLASH_INTERVAL_MS * 1000) {
      g_flash_state = !g_flash_state;
      g_last_flash_toggle_us = now_us;
    }

    uint8_t r, g, b;
    if (g_flash_state) {
      r = COLOR_RED_R;
      g = COLOR_RED_G;
      b = COLOR_RED_B;
    } else {
      r = COLOR_WHITE_R;
      g = COLOR_WHITE_G;
      b = COLOR_WHITE_B;
    }

    for (int i = 0; i < SHIFT_LIGHT_NUM_LEDS; i++) {
      led_strip_set_pixel(g_strip, i, r, g, b);
    }
    led_strip_refresh(g_strip);
    return;
  }

  /* Reset flash timer when not in flash zone */
  g_flash_state = false;
  g_last_flash_toggle_us = 0;

  /* --- Normal progressive mode --- */
  /* Count how many LEDs should be lit (0–9) */
  int num_lit = 0;
  if (rpm >= RPM_THRESHOLD_START) {
    num_lit = 1 + (rpm - RPM_THRESHOLD_START) / RPM_THRESHOLD_STEP;
    if (num_lit > RPM_THRESHOLD_COUNT) {
      num_lit = RPM_THRESHOLD_COUNT;
    }
  }

  /* Set lit LEDs left-to-right (indices 0 .. num_lit-1) */
  for (int i = 0; i < SHIFT_LIGHT_NUM_LEDS; i++) {
    if (i < num_lit) {
      led_strip_set_pixel(g_strip, i, led_colors[i][0], led_colors[i][1],
                          led_colors[i][2]);
    } else {
      led_strip_set_pixel(g_strip, i, 0, 0, 0);
    }
  }
  led_strip_refresh(g_strip);
}
