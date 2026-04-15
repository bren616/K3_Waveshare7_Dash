#include "../ui.h"

// Variables
lv_obj_t * ui_Screen2 = NULL;
lv_obj_t * ui_Screen2_RPMLabel;
lv_obj_t * ui_Screen2_RPMBar;
lv_obj_t * ui_Screen2_GearLabel;
lv_obj_t * ui_Screen2_DeltaLabel;
lv_obj_t * ui_Screen2_DeltaBar;
lv_obj_t * ui_Screen2_LapTimeLabel;
lv_obj_t * ui_Screen2_BestLapLabel;

lv_obj_t * ui_Screen2_OilTempLabel;
lv_obj_t * ui_Screen2_OilTempBar;
lv_obj_t * ui_Screen2_OilPressLabel;
lv_obj_t * ui_Screen2_OilPressBar;
lv_obj_t * ui_Screen2_WaterTempLabel;
lv_obj_t * ui_Screen2_WaterTempBar;

lv_obj_t * ui_Screen2_FLTempLabel;
lv_obj_t * ui_Screen2_FLPressLabel;
lv_obj_t * ui_Screen2_FRTempLabel;
lv_obj_t * ui_Screen2_FRPressLabel;
lv_obj_t * ui_Screen2_RLTempLabel;
lv_obj_t * ui_Screen2_RLPressLabel;
lv_obj_t * ui_Screen2_RRTempLabel;
lv_obj_t * ui_Screen2_RRPressLabel;

/* ── colours ────────────────────────────────────── */
#define COL_BG        0x000000
#define COL_BOX_BG    0x18181B  /* zinc-900 */
#define COL_BOX_BRD   0x27272A  /* zinc-800 */
#define COL_TITLE     0x71717A  /* zinc-500 */
#define COL_WHITE     0xFFFFFF
#define COL_PRIMARY   0xFAF837  /* yellow accent */
#define COL_GREEN     0x22C55E
#define COL_RED       0xEF4444
#define COL_BAR_BG    0x27272A

/* ── layout constants (1024 × 600) ──────────────── */
#define PAD          16        /* outer padding */
#define GAP          10        /* gap between elements */
#define TOP_H        88        /* RPM + Delta header row height */
#define BOT_H        105       /* tire strip height */
/* middle height is 600 - PAD*2 - TOP_H - BOT_H - GAP*2 */
#define MID_H        (600 - PAD*2 - TOP_H - BOT_H - GAP*2)
#define LEFT_W       230       /* left gauge column */
#define RIGHT_W      230       /* right timing column */
#define CENTER_W     (1024 - PAD*2 - LEFT_W - RIGHT_W - GAP*2)

/* ── helpers ────────────────────────────────────── */

/* Make an lv_obj look like a transparent container (no bg, no border, no pad) */
static void make_container(lv_obj_t *obj) {
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

/* Dark rounded card used for gauge boxes and timing boxes */
static lv_obj_t * make_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, lv_color_hex(COL_BOX_BG), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COL_BOX_BRD), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

/* Small title label (e.g. "OIL P") */
static lv_obj_t * add_title(lv_obj_t *parent, const char *text) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TITLE), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(lbl, text);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    return lbl;
}

/* Thin bar inside a gauge card */
static lv_obj_t * add_bar(lv_obj_t *parent, int32_t min, int32_t max,
                           lv_color_t ind_color, lv_coord_t w) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, w, 6);
    lv_bar_set_range(bar, min, max);
    lv_bar_set_value(bar, min, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COL_BAR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, ind_color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_img_src(bar, NULL, LV_PART_MAIN);
    lv_obj_set_style_bg_img_src(bar, NULL, LV_PART_INDICATOR);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return bar;
}

static void event_handler_swipe(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
        lv_disp_load_scr(ui_Screen1);
    }
}

/* ============================================================
 *  SCREEN INIT
 * ============================================================ */
void ui_Screen2_screen_init(void) {

    /* ── root screen ───────────────────────────────── */
    ui_Screen2 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen2, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(ui_Screen2, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(ui_Screen2, PAD, 0);
    lv_obj_add_event_cb(ui_Screen2, event_handler_swipe, LV_EVENT_GESTURE, NULL);

    /* ================================================================
     *  TOP ROW  –  RPM bar (left half)  |  Delta bar (right half)
     *  Height: TOP_H
     * ================================================================ */
    lv_obj_t *top_row = lv_obj_create(ui_Screen2);
    make_container(top_row);
    lv_obj_set_size(top_row, 1024 - PAD*2, TOP_H);
    lv_obj_align(top_row, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_coord_t half_w = (1024 - PAD*2 - GAP) / 2;

    /* ── RPM section (top-left) ──────────────────── */
    lv_obj_t *rpm_section = lv_obj_create(top_row);
    make_container(rpm_section);
    lv_obj_set_size(rpm_section, half_w, TOP_H);
    lv_obj_align(rpm_section, LV_ALIGN_TOP_LEFT, 0, 0);

    /* RPM bar */
    ui_Screen2_RPMBar = lv_bar_create(rpm_section);
    lv_obj_set_size(ui_Screen2_RPMBar, half_w, 36);
    lv_obj_align(ui_Screen2_RPMBar, LV_ALIGN_TOP_LEFT, 0, 10);
    lv_bar_set_range(ui_Screen2_RPMBar, 0, 14000);
    lv_bar_set_value(ui_Screen2_RPMBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(ui_Screen2_RPMBar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_Screen2_RPMBar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui_Screen2_RPMBar, lv_color_hex(COL_BOX_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Screen2_RPMBar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui_Screen2_RPMBar, lv_color_hex(COL_BOX_BRD), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_Screen2_RPMBar, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Screen2_RPMBar, lv_color_hex(COL_PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui_Screen2_RPMBar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(ui_Screen2_RPMBar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_img_src(ui_Screen2_RPMBar, NULL, LV_PART_MAIN);
    lv_obj_set_style_bg_img_src(ui_Screen2_RPMBar, NULL, LV_PART_INDICATOR);

    /* RPM label + caption row */
    lv_obj_t *rpm_info = lv_obj_create(rpm_section);
    make_container(rpm_info);
    lv_obj_set_size(rpm_info, half_w, 30);
    lv_obj_align(rpm_info, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *rpm_caption = lv_label_create(rpm_info);
    lv_obj_set_style_text_color(rpm_caption, lv_color_hex(COL_TITLE), 0);
    lv_obj_set_style_text_font(rpm_caption, &lv_font_montserrat_14, 0);
    lv_label_set_text(rpm_caption, "RPM x1000");
    lv_obj_align(rpm_caption, LV_ALIGN_LEFT_MID, 0, 0);

    ui_Screen2_RPMLabel = lv_label_create(rpm_info);
    lv_obj_set_style_text_color(ui_Screen2_RPMLabel, lv_color_hex(COL_PRIMARY), 0);
    lv_obj_set_style_text_font(ui_Screen2_RPMLabel, &lv_font_montserrat_24, 0);
    lv_label_set_text(ui_Screen2_RPMLabel, "0");
    lv_obj_align(ui_Screen2_RPMLabel, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ── Delta / Predictive section (top-right) ──── */
    lv_obj_t *delta_section = lv_obj_create(top_row);
    make_container(delta_section);
    lv_obj_set_size(delta_section, half_w, TOP_H);
    lv_obj_align(delta_section, LV_ALIGN_TOP_RIGHT, 0, 0);

    /* Delta bar */
    ui_Screen2_DeltaBar = lv_bar_create(delta_section);
    lv_obj_set_size(ui_Screen2_DeltaBar, half_w, 36);
    lv_obj_align(ui_Screen2_DeltaBar, LV_ALIGN_TOP_LEFT, 0, 10);
    lv_bar_set_range(ui_Screen2_DeltaBar, -500, 500);
    lv_bar_set_value(ui_Screen2_DeltaBar, 0, LV_ANIM_OFF);
    lv_bar_set_start_value(ui_Screen2_DeltaBar, 0, LV_ANIM_OFF); /* start from center */
    lv_obj_set_style_radius(ui_Screen2_DeltaBar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_Screen2_DeltaBar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui_Screen2_DeltaBar, lv_color_hex(COL_BOX_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_Screen2_DeltaBar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui_Screen2_DeltaBar, lv_color_hex(COL_BOX_BRD), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_Screen2_DeltaBar, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Screen2_DeltaBar, lv_color_hex(COL_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui_Screen2_DeltaBar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(ui_Screen2_DeltaBar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_img_src(ui_Screen2_DeltaBar, NULL, LV_PART_MAIN);
    lv_obj_set_style_bg_img_src(ui_Screen2_DeltaBar, NULL, LV_PART_INDICATOR);

    /* Delta label + caption row */
    lv_obj_t *delta_info = lv_obj_create(delta_section);
    make_container(delta_info);
    lv_obj_set_size(delta_info, half_w, 30);
    lv_obj_align(delta_info, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *delta_caption = lv_label_create(delta_info);
    lv_obj_set_style_text_color(delta_caption, lv_color_hex(COL_TITLE), 0);
    lv_obj_set_style_text_font(delta_caption, &lv_font_montserrat_14, 0);
    lv_label_set_text(delta_caption, "PREDICTIVE");
    lv_obj_align(delta_caption, LV_ALIGN_LEFT_MID, 0, 0);

    ui_Screen2_DeltaLabel = lv_label_create(delta_info);
    lv_obj_set_style_text_color(ui_Screen2_DeltaLabel, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(ui_Screen2_DeltaLabel, &lv_font_montserrat_24, 0);
    lv_label_set_text(ui_Screen2_DeltaLabel, "--");
    lv_obj_align(ui_Screen2_DeltaLabel, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ================================================================
     *  MIDDLE ROW  –  Left gauges | Center gear | Right timing
     *  Height: MID_H, positioned below top_row
     * ================================================================ */
    lv_obj_t *mid_row = lv_obj_create(ui_Screen2);
    make_container(mid_row);
    lv_obj_set_size(mid_row, 1024 - PAD*2, MID_H);
    lv_obj_align_to(mid_row, top_row, LV_ALIGN_OUT_BOTTOM_LEFT, 0, GAP);

    lv_coord_t gauge_card_h = (MID_H - GAP*2) / 3;  /* 3 cards evenly spaced */

    /* ── Left column: Oil P, Oil T, H2O ──────────── */
    lv_obj_t *left_col = lv_obj_create(mid_row);
    make_container(left_col);
    lv_obj_set_size(left_col, LEFT_W, MID_H);
    lv_obj_align(left_col, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Oil Pressure card */
    lv_obj_t *oil_p_card = make_card(left_col, LEFT_W, gauge_card_h);
    lv_obj_align(oil_p_card, LV_ALIGN_TOP_LEFT, 0, 0);
    add_title(oil_p_card, "OIL P");
    ui_Screen2_OilPressLabel = lv_label_create(oil_p_card);
    lv_obj_set_style_text_color(ui_Screen2_OilPressLabel, lv_color_hex(COL_WHITE), 0);
    lv_obj_set_style_text_font(ui_Screen2_OilPressLabel, &lv_font_montserrat_38, 0);
    lv_label_set_text(ui_Screen2_OilPressLabel, "0");
    lv_obj_align(ui_Screen2_OilPressLabel, LV_ALIGN_LEFT_MID, 0, 4);
    ui_Screen2_OilPressBar = add_bar(oil_p_card, 0, 100, lv_color_hex(COL_GREEN), LEFT_W - 20);

    /* Oil Temperature card */
    lv_obj_t *oil_t_card = make_card(left_col, LEFT_W, gauge_card_h);
    lv_obj_align(oil_t_card, LV_ALIGN_TOP_LEFT, 0, gauge_card_h + GAP);
    add_title(oil_t_card, "OIL T");
    ui_Screen2_OilTempLabel = lv_label_create(oil_t_card);
    lv_obj_set_style_text_color(ui_Screen2_OilTempLabel, lv_color_hex(COL_PRIMARY), 0);
    lv_obj_set_style_text_font(ui_Screen2_OilTempLabel, &lv_font_montserrat_38, 0);
    lv_label_set_text(ui_Screen2_OilTempLabel, "0");
    lv_obj_align(ui_Screen2_OilTempLabel, LV_ALIGN_LEFT_MID, 0, 4);
    ui_Screen2_OilTempBar = add_bar(oil_t_card, 0, 150, lv_color_hex(COL_PRIMARY), LEFT_W - 20);

    /* Water Temperature card */
    lv_obj_t *h2o_card = make_card(left_col, LEFT_W, gauge_card_h);
    lv_obj_align(h2o_card, LV_ALIGN_TOP_LEFT, 0, (gauge_card_h + GAP) * 2);
    add_title(h2o_card, "WATER");
    ui_Screen2_WaterTempLabel = lv_label_create(h2o_card);
    lv_obj_set_style_text_color(ui_Screen2_WaterTempLabel, lv_color_hex(COL_WHITE), 0);
    lv_obj_set_style_text_font(ui_Screen2_WaterTempLabel, &lv_font_montserrat_38, 0);
    lv_label_set_text(ui_Screen2_WaterTempLabel, "0");
    lv_obj_align(ui_Screen2_WaterTempLabel, LV_ALIGN_LEFT_MID, 0, 4);
    ui_Screen2_WaterTempBar = add_bar(h2o_card, 40, 120, lv_color_hex(COL_GREEN), LEFT_W - 20);

    /* ── Center column: Giant gear number ────────── */
    lv_obj_t *center_col = lv_obj_create(mid_row);
    make_container(center_col);
    lv_obj_set_size(center_col, CENTER_W, MID_H);
    lv_obj_align(center_col, LV_ALIGN_TOP_LEFT, LEFT_W + GAP, 0);

    ui_Screen2_GearLabel = lv_label_create(center_col);
    lv_obj_set_width(ui_Screen2_GearLabel, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Screen2_GearLabel, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(ui_Screen2_GearLabel, &ui_font_Font2, 0);
    lv_obj_set_style_text_color(ui_Screen2_GearLabel, lv_color_hex(COL_PRIMARY), 0);
    /* ui_font_Font2 only has glyphs 1-6.
       With montserrat_44 we can show "N" for neutral. */
    lv_label_set_text(ui_Screen2_GearLabel, "N");
    lv_obj_align(ui_Screen2_GearLabel, LV_ALIGN_CENTER, 0, -20);

    /* "GEAR" badge below the number */
    lv_obj_t *gear_badge = lv_obj_create(center_col);
    lv_obj_set_size(gear_badge, 100, 28);
    lv_obj_set_style_bg_color(gear_badge, lv_color_hex(COL_PRIMARY), 0);
    lv_obj_set_style_bg_opa(gear_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(gear_badge, 4, 0);
    lv_obj_set_style_border_width(gear_badge, 0, 0);
    lv_obj_clear_flag(gear_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(gear_badge, LV_ALIGN_CENTER, 0, MID_H / 2 - 30);

    lv_obj_t *gear_badge_lbl = lv_label_create(gear_badge);
    lv_obj_set_style_text_color(gear_badge_lbl, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_text_font(gear_badge_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(gear_badge_lbl, "GEAR");
    lv_obj_center(gear_badge_lbl);

    /* ── Right column: Lap Time, Delta, Best Lap ─── */
    lv_obj_t *right_col = lv_obj_create(mid_row);
    make_container(right_col);
    lv_obj_set_size(right_col, RIGHT_W, MID_H);
    lv_obj_align(right_col, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_coord_t timing_big_h = (MID_H - GAP*2) * 2 / 5;  /* bigger cards */
    lv_coord_t timing_small_h = MID_H - timing_big_h*2 - GAP*2;

    /* Lap Time card */
    lv_obj_t *lap_card = make_card(right_col, RIGHT_W, timing_big_h);
    lv_obj_align(lap_card, LV_ALIGN_TOP_LEFT, 0, 0);
    add_title(lap_card, "LAP TIME");
    ui_Screen2_LapTimeLabel = lv_label_create(lap_card);
    lv_obj_set_style_text_color(ui_Screen2_LapTimeLabel, lv_color_hex(COL_WHITE), 0);
    lv_obj_set_style_text_font(ui_Screen2_LapTimeLabel, &lv_font_montserrat_38, 0);
    lv_label_set_text(ui_Screen2_LapTimeLabel, "0:00.00");
    lv_obj_align(ui_Screen2_LapTimeLabel, LV_ALIGN_LEFT_MID, 0, 6);

    /* Delta card (with green right accent) */
    lv_obj_t *delta_card = make_card(right_col, RIGHT_W, timing_big_h);
    lv_obj_align(delta_card, LV_ALIGN_TOP_LEFT, 0, timing_big_h + GAP);
    add_title(delta_card, "DELTA");
    /* Green accent strip on right edge */
    lv_obj_t *delta_accent = lv_obj_create(delta_card);
    lv_obj_set_size(delta_accent, 3, timing_big_h - 4);
    lv_obj_set_style_bg_color(delta_accent, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_bg_opa(delta_accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(delta_accent, 0, 0);
    lv_obj_set_style_radius(delta_accent, 0, 0);
    lv_obj_align(delta_accent, LV_ALIGN_RIGHT_MID, 4, 0);

    /* Reuse DeltaLabel for the right-column delta readout too */
    lv_obj_t *delta_time_lbl = lv_label_create(delta_card);
    lv_obj_set_style_text_color(delta_time_lbl, lv_color_hex(COL_GREEN), 0);
    lv_obj_set_style_text_font(delta_time_lbl, &lv_font_montserrat_38, 0);
    lv_label_set_text(delta_time_lbl, "--");
    lv_obj_align(delta_time_lbl, LV_ALIGN_LEFT_MID, 0, 6);
    /* NOTE: We won't create a separate pointer for this – the BLE task
       updates ui_Screen2_DeltaLabel (in the header) which is the primary.
       If you also want this one updated, we can add it later. */

    /* Best Lap card */
    lv_obj_t *best_card = make_card(right_col, RIGHT_W, timing_small_h);
    lv_obj_align(best_card, LV_ALIGN_TOP_LEFT, 0, timing_big_h*2 + GAP*2);
    add_title(best_card, "BEST LAP");
    ui_Screen2_BestLapLabel = lv_label_create(best_card);
    lv_obj_set_style_text_color(ui_Screen2_BestLapLabel, lv_color_hex(COL_TITLE), 0);
    lv_obj_set_style_text_font(ui_Screen2_BestLapLabel, &lv_font_montserrat_24, 0);
    lv_label_set_text(ui_Screen2_BestLapLabel, "0:00.00");
    lv_obj_align(ui_Screen2_BestLapLabel, LV_ALIGN_LEFT_MID, 0, 4);

    /* ================================================================
     *  BOTTOM ROW  –  4 tire boxes:  FL  |  RL  |  FR  |  RR
     *  Height: BOT_H
     * ================================================================ */
    lv_obj_t *bot_row = lv_obj_create(ui_Screen2);
    make_container(bot_row);
    lv_obj_set_size(bot_row, 1024 - PAD*2, BOT_H);
    lv_obj_align_to(bot_row, mid_row, LV_ALIGN_OUT_BOTTOM_LEFT, 0, GAP);

    lv_coord_t tire_w = (1024 - PAD*2 - GAP*3) / 4;

    /* Helper macro for tire cards */
    #define TIRE_CARD(parent, pos_x, title_str, border_col,                  \
                      temp_ptr, press_ptr)                                    \
    do {                                                                      \
        lv_obj_t *tc = make_card(parent, tire_w, BOT_H);                     \
        lv_obj_align(tc, LV_ALIGN_TOP_LEFT, pos_x, 0);                      \
        lv_obj_set_style_border_color(tc, lv_color_hex(border_col), 0);      \
        lv_obj_set_style_border_width(tc, 2, 0);                             \
        /* Title in top-right corner */                                       \
        lv_obj_t *tl = lv_label_create(tc);                                  \
        lv_obj_set_style_text_color(tl, lv_color_hex(COL_TITLE), 0);         \
        lv_obj_set_style_text_font(tl, &lv_font_montserrat_14, 0);           \
        lv_label_set_text(tl, title_str);                                    \
        lv_obj_align(tl, LV_ALIGN_TOP_RIGHT, 0, 0);                         \
        /* Temperature value */                                               \
        temp_ptr = lv_label_create(tc);                                      \
        lv_obj_set_style_text_color(temp_ptr, lv_color_hex(COL_WHITE), 0);   \
        lv_obj_set_style_text_font(temp_ptr, &lv_font_montserrat_24, 0);     \
        lv_label_set_text(temp_ptr, "0 °C");                                 \
        lv_obj_align(temp_ptr, LV_ALIGN_TOP_LEFT, 0, 22);                   \
        /* Pressure value */                                                  \
        press_ptr = lv_label_create(tc);                                     \
        lv_obj_set_style_text_color(press_ptr, lv_color_hex(COL_PRIMARY), 0);\
        lv_obj_set_style_text_font(press_ptr, &lv_font_montserrat_24, 0);    \
        lv_label_set_text(press_ptr, "0 PSI");                               \
        lv_obj_align(press_ptr, LV_ALIGN_BOTTOM_LEFT, 0, 0);                \
    } while (0)

    TIRE_CARD(bot_row, 0,
              "FL", COL_PRIMARY,
              ui_Screen2_FLTempLabel, ui_Screen2_FLPressLabel);

    TIRE_CARD(bot_row, tire_w + GAP,
              "RL", COL_PRIMARY,
              ui_Screen2_RLTempLabel, ui_Screen2_RLPressLabel);

    TIRE_CARD(bot_row, (tire_w + GAP) * 2,
              "FR", COL_PRIMARY,
              ui_Screen2_FRTempLabel, ui_Screen2_FRPressLabel);

    TIRE_CARD(bot_row, (tire_w + GAP) * 3,
              "RR", COL_PRIMARY,
              ui_Screen2_RRTempLabel, ui_Screen2_RRPressLabel);

    #undef TIRE_CARD
}

void ui_Screen2_screen_destroy(void) {
    if (ui_Screen2) {
        lv_obj_del(ui_Screen2);
        ui_Screen2 = NULL;
    }
}
