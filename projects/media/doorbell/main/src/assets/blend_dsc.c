#include "bk_draw_blend.h"

DRAW_IMG_DECLARE(font_clock)
DRAW_IMG_DECLARE(font_dates)
DRAW_IMG_DECLARE(font_weather)
DRAW_IMG_DECLARE(font_ver)
DRAW_IMG_DECLARE(font_week)
DRAW_IMG_DECLARE(wifi_0_argb8888)
DRAW_IMG_DECLARE(wifi_1_argb8888)
DRAW_IMG_DECLARE(wifi_2_argb8888)
DRAW_IMG_DECLARE(wifi_3_argb8888)
DRAW_IMG_DECLARE(wifi_full_argb8888)
DRAW_IMG_DECLARE(img_battery_1)
DRAW_IMG_DECLARE(img_cloudy_to_sunny)


GUI_CONST_STORAGE blend_info_t blend_info[8] =
{
    {.addr = &font_clock, .content = "12:30"},
    {.addr = &font_dates, .content = "2025/1/2 周四"},
    {.addr = &font_ver, .content = "v 1.0.0"},
    {.addr = &wifi_1_argb8888, .content = "wifi2"},
    {.addr = &img_battery_1, .content = "battery_1"},
    {.addr = &img_cloudy_to_sunny, .content = "cloudy_to_sunny"},
};

