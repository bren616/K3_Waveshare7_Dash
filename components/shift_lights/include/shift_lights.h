#ifndef SHIFT_LIGHTS_H
#define SHIFT_LIGHTS_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the WS2812B shift light strip via RMT.
 * @return ESP_OK on success.
 */
esp_err_t shift_lights_init(void);

/**
 * @brief Update the shift light LEDs based on current RPM.
 *
 * Call this from the CAN RX path whenever a new RPM value arrives.
 * Below 9000 RPM all LEDs are off. From 9000–13000 RPM LEDs light
 * left-to-right in Blue/Orange/Red bands. Above 13000 RPM all 10
 * LEDs flash Red/White at 500 ms intervals.
 *
 * @param rpm Current engine RPM value.
 */
void shift_lights_update(int32_t rpm);

/* ---------- Testable computation API ---------- */

#define SHIFT_LIGHT_NUM_LEDS 10

/** Computed state for a single LED. */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} shift_lights_pixel_t;

/** Full LED strip computed state. */
typedef struct {
    shift_lights_pixel_t pixels[SHIFT_LIGHT_NUM_LEDS];
    bool flash_state;           /**< Current flash toggle (input/output) */
    int64_t last_flash_toggle_us; /**< Timestamp of last toggle (input/output) */
} shift_lights_led_state_t;

/**
 * @brief Pure computation: given RPM and time, compute what each LED should show.
 *
 * This function has NO hardware side-effects and is safe to call from tests.
 * On entry, state->flash_state and state->last_flash_toggle_us should carry
 * the values from the previous call (or be zeroed for the first call).
 *
 * @param rpm     Current engine RPM.
 * @param now_us  Current time in microseconds (e.g. from esp_timer_get_time).
 * @param state   [in/out] LED state buffer.
 */
void shift_lights_compute(int32_t rpm, int64_t now_us,
                          shift_lights_led_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* SHIFT_LIGHTS_H */
