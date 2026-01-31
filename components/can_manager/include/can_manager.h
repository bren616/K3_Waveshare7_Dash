#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Structure to define a mapped variable
typedef struct {
  const char *name;        // Name for logging
  lv_obj_t **lv_label_ptr; // Pointer to the LVGL label object pointer (e.g.
                           // &ui_RPMLabel)
  uint32_t can_id;         // CAN ID to listen for
  uint8_t num_bytes;       // 1 or 2
  int8_t msb_idx;          // Index of MSB (or -1 if single byte)
  int8_t lsb_idx;          // Index of LSB

  // Mock Data parameters
  int32_t current_val;
  int32_t min_val;
  int32_t max_val;
  int32_t step_val;
  bool increasing; // Direction for mock sweep
} DashVariable;

/**
 * @brief Initialize the CAN manager
 * Starts TWAI driver and creates RX/Mock tasks.
 */
void init_can_manager(void);

/**
 * @brief Set the dashboard variables configuration
 * @param vars Pointer to array of DashVariable
 * @param count Number of variables
 */
void set_dash_variables(DashVariable *vars, size_t count);

#ifdef __cplusplus
}
#endif

#endif // CAN_MANAGER_H
