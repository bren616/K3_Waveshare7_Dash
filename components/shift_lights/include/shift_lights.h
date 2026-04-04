#ifndef SHIFT_LIGHTS_H
#define SHIFT_LIGHTS_H

#include "esp_err.h"
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

#ifdef __cplusplus
}
#endif

#endif /* SHIFT_LIGHTS_H */
