#ifndef UI_SCREEN2_H
#define UI_SCREEN2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// SCREEN: ui_Screen2
extern void ui_Screen2_screen_init(void);
extern void ui_Screen2_screen_destroy(void);
extern lv_obj_t * ui_Screen2;

extern lv_obj_t * ui_Screen2_RPMLabel;
extern lv_obj_t * ui_Screen2_RPMBar;
extern lv_obj_t * ui_Screen2_GearLabel;
extern lv_obj_t * ui_Screen2_DeltaLabel;
extern lv_obj_t * ui_Screen2_DeltaBar;
extern lv_obj_t * ui_Screen2_LapTimeLabel;
extern lv_obj_t * ui_Screen2_BestLapLabel;

extern lv_obj_t * ui_Screen2_OilTempLabel;
extern lv_obj_t * ui_Screen2_OilTempBar;
extern lv_obj_t * ui_Screen2_OilPressLabel;
extern lv_obj_t * ui_Screen2_OilPressBar;
extern lv_obj_t * ui_Screen2_WaterTempLabel;
extern lv_obj_t * ui_Screen2_WaterTempBar;

extern lv_obj_t * ui_Screen2_FLTempLabel;
extern lv_obj_t * ui_Screen2_FLPressLabel;
extern lv_obj_t * ui_Screen2_FRTempLabel;
extern lv_obj_t * ui_Screen2_FRPressLabel;
extern lv_obj_t * ui_Screen2_RLTempLabel;
extern lv_obj_t * ui_Screen2_RLPressLabel;
extern lv_obj_t * ui_Screen2_RRTempLabel;
extern lv_obj_t * ui_Screen2_RRPressLabel;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
