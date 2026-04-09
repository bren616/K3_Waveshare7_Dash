/**
 * @file test_shift_lights.c
 * @brief Unity tests for shift_lights_compute() — the pure LED calculation logic.
 *
 * These tests call shift_lights_compute() directly, which has zero hardware
 * dependencies. They verify RPM-to-LED colour mapping, progressive illumination,
 * flash mode, and edge cases.
 *
 * Run via:  idf.py -T shift_lights build flash monitor
 */

#include "unity.h"
#include "shift_lights.h"
#include <string.h>

/* ---- Helper colours (must match shift_lights.c definitions) ---- */
#define BLUE_R   0
#define BLUE_G   0
#define BLUE_B   255

#define ORANGE_R 255
#define ORANGE_G 165
#define ORANGE_B 0

#define RED_R    255
#define RED_G    0
#define RED_B    0

#define WHITE_R  255
#define WHITE_G  255
#define WHITE_B  255

/* ---- Convenience helpers ---- */

static shift_lights_led_state_t state;

/** Zero the state struct before each test. */
static void reset_state(void)
{
    memset(&state, 0, sizeof(state));
}

/** Assert a single pixel has the expected RGB values. */
static void assert_pixel(int led, uint8_t r, uint8_t g, uint8_t b,
                          const char *label)
{
    char msg[128];
    snprintf(msg, sizeof(msg), "%s — LED %d R (expected %u, got %u)",
             label, led, r, state.pixels[led].r);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(r, state.pixels[led].r, msg);

    snprintf(msg, sizeof(msg), "%s — LED %d G (expected %u, got %u)",
             label, led, g, state.pixels[led].g);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(g, state.pixels[led].g, msg);

    snprintf(msg, sizeof(msg), "%s — LED %d B (expected %u, got %u)",
             label, led, b, state.pixels[led].b);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(b, state.pixels[led].b, msg);
}

/** Assert that a pixel is off (0,0,0). */
static void assert_pixel_off(int led, const char *label)
{
    assert_pixel(led, 0, 0, 0, label);
}

/** Count how many LEDs have any non-zero colour. */
static int count_lit(void)
{
    int n = 0;
    for (int i = 0; i < SHIFT_LIGHT_NUM_LEDS; i++) {
        if (state.pixels[i].r || state.pixels[i].g || state.pixels[i].b)
            n++;
    }
    return n;
}

/* ================================================================== */
/*  Test cases                                                         */
/* ================================================================== */

/* ---- 1. All LEDs off below 9000 RPM ---- */
TEST_CASE("All LEDs off below 9000 RPM", "[shift_lights]")
{
    int32_t test_rpms[] = {0, 1000, 4500, 8999};
    for (int t = 0; t < 4; t++) {
        reset_state();
        shift_lights_compute(test_rpms[t], 0, &state);
        for (int i = 0; i < SHIFT_LIGHT_NUM_LEDS; i++) {
            assert_pixel_off(i, "Below 9000");
        }
    }
}

/* ---- 2. One blue LED at exactly 9000 RPM ---- */
TEST_CASE("One blue LED at 9000 RPM", "[shift_lights]")
{
    reset_state();
    shift_lights_compute(9000, 0, &state);

    assert_pixel(0, BLUE_R, BLUE_G, BLUE_B, "9000 RPM LED0");
    for (int i = 1; i < SHIFT_LIGHT_NUM_LEDS; i++) {
        assert_pixel_off(i, "9000 RPM rest");
    }
}

/* ---- 3. Two blue LEDs at 9500 RPM ---- */
TEST_CASE("Two blue LEDs at 9500 RPM", "[shift_lights]")
{
    reset_state();
    shift_lights_compute(9500, 0, &state);

    assert_pixel(0, BLUE_R, BLUE_G, BLUE_B, "9500 RPM LED0");
    assert_pixel(1, BLUE_R, BLUE_G, BLUE_B, "9500 RPM LED1");
    for (int i = 2; i < SHIFT_LIGHT_NUM_LEDS; i++) {
        assert_pixel_off(i, "9500 RPM rest");
    }
}

/* ---- 4. Progressive: 4 LEDs at 10500 RPM (3 Blue + 1 Orange) ---- */
TEST_CASE("Four LEDs at 10500 RPM", "[shift_lights]")
{
    reset_state();
    /* 10500: 1 + (10500 - 9000)/500 = 4 LEDs */
    shift_lights_compute(10500, 0, &state);

    /* LEDs 0-2 = Blue */
    assert_pixel(0, BLUE_R, BLUE_G, BLUE_B, "10500 LED0");
    assert_pixel(1, BLUE_R, BLUE_G, BLUE_B, "10500 LED1");
    assert_pixel(2, BLUE_R, BLUE_G, BLUE_B, "10500 LED2");
    /* LED 3 = Orange */
    assert_pixel(3, ORANGE_R, ORANGE_G, ORANGE_B, "10500 LED3");
    /* LEDs 4-9 off */
    for (int i = 4; i < SHIFT_LIGHT_NUM_LEDS; i++) {
        assert_pixel_off(i, "10500 rest");
    }
}

/* ---- 5. Full 9 LEDs at 13000 RPM (not in flash mode) ---- */
TEST_CASE("Nine LEDs at 13000 RPM, no flash", "[shift_lights]")
{
    reset_state();
    /* 13000: 1 + (13000-9000)/500 = 9 LEDs. 13000 == threshold, not > */
    shift_lights_compute(13000, 0, &state);

    /* LEDs 0-2 Blue */
    for (int i = 0; i < 3; i++) {
        assert_pixel(i, BLUE_R, BLUE_G, BLUE_B, "13000 Blue");
    }
    /* LEDs 3-5 Orange */
    for (int i = 3; i < 6; i++) {
        assert_pixel(i, ORANGE_R, ORANGE_G, ORANGE_B, "13000 Orange");
    }
    /* LEDs 6-8 Red */
    for (int i = 6; i < 9; i++) {
        assert_pixel(i, RED_R, RED_G, RED_B, "13000 Red");
    }
    /* LED 9 off (only in flash mode) */
    assert_pixel_off(9, "13000 LED9");
}

/* ---- 6. Flash mode activates above 13000 RPM ---- */
TEST_CASE("Flash mode above 13000 RPM - initial call", "[shift_lights]")
{
    reset_state();
    shift_lights_compute(13500, 0, &state);

    /* All 10 LEDs should be one of {RED, WHITE} */
    uint8_t r = state.pixels[0].r;
    uint8_t g = state.pixels[0].g;
    uint8_t b = state.pixels[0].b;

    bool is_red   = (r == RED_R && g == RED_G && b == RED_B);
    bool is_white = (r == WHITE_R && g == WHITE_G && b == WHITE_B);
    TEST_ASSERT_TRUE_MESSAGE(is_red || is_white,
                             "Flash LED0 should be red or white");

    /* All LEDs should be the same colour */
    for (int i = 1; i < SHIFT_LIGHT_NUM_LEDS; i++) {
        TEST_ASSERT_EQUAL_UINT8(r, state.pixels[i].r);
        TEST_ASSERT_EQUAL_UINT8(g, state.pixels[i].g);
        TEST_ASSERT_EQUAL_UINT8(b, state.pixels[i].b);
    }
}

/* ---- 7. Flash toggles after 500 ms ---- */
TEST_CASE("Flash colour toggles after 500 ms", "[shift_lights]")
{
    reset_state();

    /* First call at time 0 */
    shift_lights_compute(14000, 0, &state);
    uint8_t first_r = state.pixels[0].r;
    uint8_t first_g = state.pixels[0].g;
    uint8_t first_b = state.pixels[0].b;

    /* Advance 500 ms (500000 µs) — should toggle */
    shift_lights_compute(14000, 500000, &state);
    uint8_t second_r = state.pixels[0].r;
    uint8_t second_g = state.pixels[0].g;
    uint8_t second_b = state.pixels[0].b;

    /* Colours must differ */
    bool changed = (first_r != second_r) || (first_g != second_g) ||
                   (first_b != second_b);
    TEST_ASSERT_TRUE_MESSAGE(changed,
                             "Flash colour should toggle after 500 ms");

    /* Second colour must also be red or white */
    bool is_red   = (second_r == RED_R && second_g == RED_G && second_b == RED_B);
    bool is_white = (second_r == WHITE_R && second_g == WHITE_G &&
                     second_b == WHITE_B);
    TEST_ASSERT_TRUE_MESSAGE(is_red || is_white,
                             "Toggled colour should be red or white");
}

/* ---- 8. Flash does NOT toggle before 500 ms ---- */
TEST_CASE("Flash does not toggle before 500 ms", "[shift_lights]")
{
    reset_state();

    shift_lights_compute(14000, 0, &state);
    uint8_t first_r = state.pixels[0].r;

    /* Only 300 ms have passed — should NOT toggle */
    shift_lights_compute(14000, 300000, &state);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(first_r, state.pixels[0].r,
                                    "Should not toggle before 500 ms");
}

/* ---- 9. Flash state resets when RPM drops below threshold ---- */
TEST_CASE("Flash state resets when RPM drops", "[shift_lights]")
{
    reset_state();

    /* Enter flash mode */
    shift_lights_compute(14000, 0, &state);
    TEST_ASSERT_TRUE(state.flash_state);

    /* Drop to 10000 RPM — flash_state should clear */
    shift_lights_compute(10000, 1000000, &state);
    TEST_ASSERT_FALSE(state.flash_state);
    TEST_ASSERT_EQUAL_INT64(0, state.last_flash_toggle_us);

    /* Verify correct progressive LED pattern: 3 LEDs lit */
    /* 10000: 1 + (10000-9000)/500 = 3 */
    assert_pixel(0, BLUE_R, BLUE_G, BLUE_B, "Post-flash LED0");
    assert_pixel(1, BLUE_R, BLUE_G, BLUE_B, "Post-flash LED1");
    assert_pixel(2, BLUE_R, BLUE_G, BLUE_B, "Post-flash LED2");
    for (int i = 3; i < SHIFT_LIGHT_NUM_LEDS; i++) {
        assert_pixel_off(i, "Post-flash rest");
    }
}

/* ---- 10. Negative RPM treated as 0 ---- */
TEST_CASE("Negative RPM turns all LEDs off", "[shift_lights]")
{
    reset_state();
    shift_lights_compute(-500, 0, &state);
    for (int i = 0; i < SHIFT_LIGHT_NUM_LEDS; i++) {
        assert_pixel_off(i, "Negative RPM");
    }
}

/* ---- 11. LED count at every 500 RPM boundary ---- */
TEST_CASE("Correct LED count at each 500 RPM step", "[shift_lights]")
{
    struct { int32_t rpm; int expected_lit; } cases[] = {
        {  8999,  0 },
        {  9000,  1 },
        {  9499,  1 },
        {  9500,  2 },
        { 10000,  3 },
        { 10500,  4 },
        { 11000,  5 },
        { 11500,  6 },
        { 12000,  7 },
        { 12500,  8 },
        { 13000,  9 },  /* max in progressive mode */
    };

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        reset_state();
        shift_lights_compute(cases[c].rpm, 0, &state);
        int lit = count_lit();

        char msg[80];
        snprintf(msg, sizeof(msg), "RPM %ld => expected %d LEDs, got %d",
                 (long)cases[c].rpm, cases[c].expected_lit, lit);
        TEST_ASSERT_EQUAL_INT_MESSAGE(cases[c].expected_lit, lit, msg);
    }
}

/* ---- 12. Mid-range value within a step doesn't jump ---- */
TEST_CASE("RPM mid-step (e.g. 9250) does not light extra LED", "[shift_lights]")
{
    reset_state();
    /* 9250: 1 + (9250-9000)/500 = 1 + 0 = 1 (integer division) */
    shift_lights_compute(9250, 0, &state);
    TEST_ASSERT_EQUAL_INT(1, count_lit());
    assert_pixel(0, BLUE_R, BLUE_G, BLUE_B, "9250 RPM LED0");
}
