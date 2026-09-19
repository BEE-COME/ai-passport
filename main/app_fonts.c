#include "app_fonts.h"

lv_font_t app_font_16_fb;
lv_font_t app_font_20_fb;

void app_fonts_init(void)
{
    app_font_16_fb = app_font_16;
    app_font_16_fb.fallback = &lv_font_montserrat_14;
    app_font_20_fb = app_font_20;
    app_font_20_fb.fallback = &lv_font_montserrat_20;
}
