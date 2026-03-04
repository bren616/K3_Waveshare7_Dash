/*
 * BLE Manager - RaceChrono DIY BLE Device
 *
 * Implements the RaceChrono DIY BLE protocol using ESP-Hosted Bluedroid
 * to receive lap time and delta data from the RaceChrono Android app.
 */

#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Initialize the BLE manager.
 * Sets up ESP-Hosted connection to ESP32-C6, initializes Bluedroid,
 * registers GATT service, and starts advertising as an RC DIY device.
 *
 * @return ESP_OK on success
 */
esp_err_t ble_manager_init(void);

/**
 * Check if a BLE client (RaceChrono) is currently connected.
 */
bool ble_manager_is_connected(void);

/**
 * Get the current delta speed value (scaled by 100).
 * @param val     Output: delta speed
 * @param valid   Output: true if a value has been received
 */
void ble_manager_get_delta_speed(int32_t *val, bool *valid);

/**
 * Get the current delta value.
 * @param val_ms  Output: delta in milliseconds
 * @param valid   Output: true if a value has been received
 */
void ble_manager_get_delta(int32_t *val_ms, bool *valid);

#ifdef __cplusplus
}
#endif

#endif /* BLE_MANAGER_H */
