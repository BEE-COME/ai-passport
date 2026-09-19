#pragma once

#include "lvgl.h"

LV_FONT_DECLARE(app_font_16);
LV_FONT_DECLARE(app_font_20);

extern lv_font_t app_font_16_fb;
extern lv_font_t app_font_20_fb;

void app_fonts_init(void);
