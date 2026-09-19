// 单词学习本：LVGL 页面、三键交互、NVS 状态与离线发音。
#include "word_app.h"

#include <stdio.h>
#include <string.h>

#include "app_fonts.h"
#include "audio_player.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "esp_random.h"
#include "lvgl.h"
#include "word_data.h"
#include "word_model.h"
#include "word_quiz.h"
#include "word_store.h"

static const char *TAG = "word_app";

#define SCREEN_W 240
#define SCREEN_H 320
#define HEADER_H 36
#define ROW_X 10
#define ROW_W 220

#define COLOR_PAPER  0xFAF6ED
#define COLOR_PAPER_D 0xF0E9DA
#define COLOR_WHITE  0xFFFDF7
#define COLOR_INK    0x1C1917
#define COLOR_SOFT   0x57534E
#define COLOR_LINE   0xE4DDCA
#define COLOR_SELECT 0xFFF8E8

static const uint32_t CAT_COLORS[5] = {
    0xB45309, 0x166534, 0xA16207, 0x1E40AF, 0x7C3AED,
};
static const uint32_t CAT_BG[5] = {
    0xFEF3C7, 0xDCFCE7, 0xFEF9C3, 0xDBEAFE, 0xEDE9FE,
};
static const uint16_t CAT_START[5] = { 0, 100, 500, 700, 800 };

typedef enum {
    SCREEN_HOME = 0,
    SCREEN_CATEGORY,
    SCREEN_LIST,
    SCREEN_DETAIL,
    SCREEN_STUDY,
    SCREEN_STUDY_DONE,
    SCREEN_QUIZ,
    SCREEN_COLLECTION,
    SCREEN_STATS,
} app_screen_t;

typedef struct {
    app_screen_t screen;
    uint8_t home_index;
    uint8_t category_index;
    uint8_t list_category;      /* 0 = all, 1..5 = category id + 1 */
    uint16_t list_selected;
    uint16_t list_offset;
    bool list_from_all;
    uint16_t detail_index;
    uint8_t study_pos;
    bool study_revealed;
    uint8_t study_deck[QUIZ_ITEM_COUNT];
    uint8_t quiz_pos;
    bool quiz_answered;
    uint8_t quiz_chosen;
    word_quiz_item_t quiz_items[QUIZ_ITEM_COUNT];
} app_state_t;

static app_state_t s_app;
static word_model_t s_model;
static lv_obj_t *s_scr;
static uint16_t s_collection[WORD_COUNT];
static uint16_t s_collection_count;
static bool s_audio_ok;

static uint16_t list_count(void)
{
    if (s_app.screen == SCREEN_COLLECTION) return s_collection_count;
    if (s_app.list_category == 0) return WORD_COUNT;
    return WORD_CATEGORY_COUNTS[s_app.list_category - 1];
}

static uint16_t list_get_index(uint16_t position)
{
    if (s_app.screen == SCREEN_COLLECTION) {
        if (position >= s_collection_count) return 0;
        return s_collection[position];
    }
    if (s_app.list_category == 0) return position;
    uint16_t start = CAT_START[s_app.list_category - 1];
    return start + position;
}

static void rebuild_collection(void)
{
    s_collection_count = 0;
    for (uint16_t i = 0; i < WORD_COUNT; i++) {
        if (word_model_is_starred(&s_model, i)) {
            s_collection[s_collection_count++] = i;
        }
    }
}

static lv_obj_t *rect(lv_obj_t *parent, int x, int y, int w, int h,
                      uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, const lv_font_t *font,
                       uint32_t color, const char *text)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_text(obj, text);
    return obj;
}

static lv_obj_t *row_box(lv_obj_t *parent, int y, int h, uint32_t bg,
                         bool selected)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, ROW_X, y);
    lv_obj_set_size(obj, ROW_W, h);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(bg), 0);
    lv_obj_set_style_border_width(obj, selected ? 2 : 1, 0);
    lv_obj_set_style_border_color(obj,
        lv_color_hex(selected ? COLOR_INK : COLOR_LINE), 0);
    return obj;
}

static void destroy_screen(void)
{
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
}

static lv_obj_t *screen_new(void)
{
    destroy_screen();
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COLOR_PAPER), 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_set_size(s_scr, SCREEN_W, SCREEN_H);
    return s_scr;
}

static void header_add(lv_obj_t *scr, const char *title)
{
    rect(scr, 0, 0, SCREEN_W, HEADER_H, COLOR_WHITE);
    rect(scr, 0, HEADER_H - 2, SCREEN_W, 2, COLOR_LINE);
    label(scr, 10, 9, &app_font_16_fb, COLOR_INK, title);

    int soc = bsp_battery_soc();
    char text[16];
    if (soc < 0) {
        snprintf(text, sizeof(text), "--");
    } else {
        snprintf(text, sizeof(text), "%d%%", soc);
    }
    lv_obj_t *battery = label(scr, 198, 10, &app_font_16_fb, COLOR_SOFT, text);
    lv_obj_set_width(battery, 34);
    lv_obj_set_style_text_align(battery, LV_TEXT_ALIGN_RIGHT, 0);
}

static void render_home(void)
{
    lv_obj_t *scr = screen_new();
    header_add(scr, "单词学习本");

    uint16_t mastered = 0, learning = 0, starred = 0;
    word_model_stats(&s_model, &mastered, &learning, &starred);
    char progress[32];
    snprintf(progress, sizeof(progress), "%u / 850 已掌握", (unsigned)mastered);
    label(scr, 12, 45, &app_font_16_fb, COLOR_SOFT, progress);
    lv_obj_t *bar = rect(scr, 12, 62, 116, 5, COLOR_PAPER_D);
    uint32_t fill_w = mastered * 116U / WORD_COUNT;
    if (fill_w > 0) rect(bar, 0, 0, (int)fill_w, 5, 0xB45309);

    static const char *const TITLES[] = {
        "分类浏览", "每日学习", "单词自测", "生词本", "学习统计", "全部单词",
    };
    static const char *const SUBS[] = {
        "六类词库", "10 词任务", "三选一", "收藏单词", "累计进度", "850 词",
    };
    static const uint32_t ACCENTS[6] = {
        0x1C1917, 0x166534, 0x1E40AF, 0xB45309, 0x7C3AED, 0x57534E,
    };
    for (int i = 0; i < 6; i++) {
        int y = 76 + i * 40;
        bool selected = i == (int)s_app.home_index;
        lv_obj_t *row = row_box(scr, y, 36,
                                selected ? COLOR_SELECT : COLOR_WHITE, selected);
        lv_obj_t *stripe = rect(row, 0, 0, 5, 36, ACCENTS[i]);
        if (selected) lv_obj_add_flag(stripe, LV_OBJ_FLAG_HIDDEN);
        label(row, 14, 9, &app_font_16_fb, COLOR_INK, TITLES[i]);
        lv_obj_t *sub = label(row, 124, 11, &app_font_16_fb, COLOR_SOFT, SUBS[i]);
        lv_obj_set_width(sub, 90);
        lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_RIGHT, 0);
    }
    lv_screen_load(scr);
}

static void render_categories(void)
{
    lv_obj_t *scr = screen_new();
    header_add(scr, "分类浏览");

    static const char *const ZH[] = {
        "全部单词", "操作词", "通用词", "图示词", "性质词", "反义对",
    };
    static const uint16_t COUNTS[] = { 850, 100, 400, 200, 100, 50 };
    for (int i = 0; i < 6; i++) {
        int y = 50 + i * 40;
        bool selected = i == (int)s_app.category_index;
        lv_obj_t *row = row_box(scr, y, 36,
                                selected ? COLOR_SELECT : COLOR_WHITE, selected);
        lv_obj_t *stripe = rect(row, 0, 0, 5, 36,
                                i == 0 ? COLOR_INK : CAT_COLORS[i - 1]);
        if (selected) lv_obj_add_flag(stripe, LV_OBJ_FLAG_HIDDEN);
        label(row, 14, 9, &app_font_16_fb, COLOR_INK, ZH[i]);
        char count[16];
        snprintf(count, sizeof(count), "%u 词", (unsigned)COUNTS[i]);
        lv_obj_t *sub = label(row, 170, 11, &app_font_16_fb, COLOR_SOFT, count);
        lv_obj_set_width(sub, 42);
        lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_RIGHT, 0);
    }
    lv_screen_load(scr);
}

static void render_word_list(void)
{
    lv_obj_t *scr = screen_new();
    const char *title = s_app.list_category == 0
                            ? "全部单词"
                            : WORD_CATEGORY_ZH[s_app.list_category - 1];
    header_add(scr, title);

    uint16_t count = list_count();
    uint16_t start = s_app.list_offset + 1;
    uint16_t end = start + 5 < count ? start + 5 : count;
    char head[32];
    snprintf(head, sizeof(head), "%u-%u / %u", (unsigned)start,
             (unsigned)end, (unsigned)count);
    label(scr, 12, 44, &app_font_16_fb, COLOR_SOFT, head);

    uint32_t accent = s_app.list_category == 0
                          ? COLOR_INK
                          : CAT_COLORS[s_app.list_category - 1];
    uint32_t bg = s_app.list_category == 0
                      ? COLOR_PAPER_D
                      : CAT_BG[s_app.list_category - 1];
    for (int i = 0; i < 6; i++) {
        uint16_t position = s_app.list_offset + (uint16_t)i;
        if (position >= count) break;
        int y = 62 + i * 38;
        bool selected = position == s_app.list_selected;
        const word_entry_t *entry = &WORD_LIST[list_get_index(position)];
        lv_obj_t *row = row_box(scr, y, 34,
                                selected ? bg : COLOR_WHITE, selected);
        char number[8];
        snprintf(number, sizeof(number), "%03u", (unsigned)(position + 1));
        label(row, 8, 9, &app_font_16_fb, COLOR_SOFT, number);
        label(row, 38, 8, &lv_font_montserrat_20, COLOR_INK, entry->w);
        lv_obj_t *zh = label(row, 132, 10, &app_font_16_fb, COLOR_SOFT, entry->zh);
        lv_obj_set_width(zh, 80);
        lv_label_set_long_mode(zh, LV_LABEL_LONG_DOT);
        (void)accent;
    }
    lv_screen_load(scr);
}

static void render_detail(void)
{
    lv_obj_t *scr = screen_new();
    header_add(scr, "单词详情");
    uint16_t word_index = list_get_index(s_app.detail_index);
    const word_entry_t *entry = &WORD_LIST[word_index];

    lv_obj_t *detail = lv_obj_create(scr);
    lv_obj_set_pos(detail, 6, 42);
    lv_obj_set_size(detail, 228, 268);
    lv_obj_set_style_bg_opa(detail, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(detail, 0, 0);
    lv_obj_set_style_pad_all(detail, 0, 0);
    lv_obj_remove_flag(detail, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(detail, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *top = label(detail, 4, 0, &lv_font_montserrat_20,
                          COLOR_INK, entry->w);
    lv_obj_set_style_text_color(top, lv_color_hex(COLOR_INK), 0);
    label(detail, 176, 1, &app_font_20_fb,
          word_model_is_starred(&s_model, word_index) ? 0xEAB308 : COLOR_SOFT,
          word_model_is_starred(&s_model, word_index) ? "★" : "☆");
    label(detail, 198, 3, &app_font_16_fb, COLOR_SOFT, "发音");

    uint8_t cat = entry->cat;
    lv_obj_t *chip_bg = rect(detail, 4, 30, 74, 18, CAT_BG[cat]);
    rect(chip_bg, 0, 0, 3, 18, CAT_COLORS[cat]);
    label(chip_bg, 9, 1, &app_font_16_fb, CAT_COLORS[cat],
          WORD_CATEGORY_ZH[cat]);

    label(detail, 84, 30, &app_font_16_fb, COLOR_INK, entry->zh);

    lv_obj_t *core_block = label(detail, 4, 58, &app_font_16_fb,
                                 COLOR_SOFT, entry->core);
    lv_obj_set_width(core_block, 218);
    lv_label_set_long_mode(core_block, LV_LABEL_LONG_WRAP);
    lv_obj_update_layout(core_block);

    int en_y = 58 + (int)lv_obj_get_height(core_block) + 8;
    lv_obj_t *en_block = label(detail, 4, en_y, &app_font_16_fb,
                               COLOR_INK, entry->en);
    lv_obj_set_width(en_block, 218);
    lv_label_set_long_mode(en_block, LV_LABEL_LONG_WRAP);
    lv_obj_update_layout(en_block);

    int ex_y = en_y + (int)lv_obj_get_height(en_block) + 8;
    lv_obj_t *ex_block = label(detail, 4, ex_y, &app_font_16_fb,
                               COLOR_INK, entry->ex);
    lv_obj_set_width(ex_block, 218);
    lv_label_set_long_mode(ex_block, LV_LABEL_LONG_WRAP);
    lv_obj_update_layout(ex_block);
    int exz_y = ex_y + (int)lv_obj_get_height(ex_block) + 1;
    lv_obj_t *exz_block = label(detail, 4, exz_y, &app_font_16_fb,
                                COLOR_SOFT, entry->exz);
    lv_obj_set_width(exz_block, 218);
    lv_label_set_long_mode(exz_block, LV_LABEL_LONG_WRAP);
    lv_obj_update_layout(exz_block);

    int syn_y = exz_y + (int)lv_obj_get_height(exz_block) + 8;
    label(detail, 4, syn_y, &app_font_16_fb, COLOR_SOFT, "近义词");
    char syn_text[96] = { 0 };
    for (int i = 0; i < 3 && entry->syn[i] && entry->syn[i][0]; i++) {
        if (i) strncat(syn_text, " / ", sizeof(syn_text) - strlen(syn_text) - 1);
        strncat(syn_text, entry->syn[i], sizeof(syn_text) - strlen(syn_text) - 1);
    }
    lv_obj_t *syn = label(detail, 4, syn_y + 18, &app_font_16_fb,
                          0x1E40AF, syn_text);
    lv_obj_set_width(syn, 218);
    lv_label_set_long_mode(syn, LV_LABEL_LONG_WRAP);
    lv_screen_load(scr);
}

static void render_study(void)
{
    lv_obj_t *scr = screen_new();
    uint16_t word_index = s_app.study_deck[s_app.study_pos];
    const word_entry_t *entry = &WORD_LIST[word_index];
    header_add(scr, "每日学习");

    char head[32];
    snprintf(head, sizeof(head), "%u / 10", (unsigned)(s_app.study_pos + 1));
    label(scr, 12, 44, &app_font_16_fb, COLOR_SOFT, head);

    lv_obj_t *card = rect(scr, 16, 62, 208, 216, COLOR_WHITE);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_INK), 0);
    if (!s_app.study_revealed) {
        lv_obj_t *word = label(card, 0, 82, &lv_font_montserrat_20,
                               COLOR_INK, entry->w);
        lv_obj_set_size(word, 208, 40);
        lv_obj_set_style_text_align(word, LV_TEXT_ALIGN_CENTER, 0);
        label(card, 76, 138, &app_font_16_fb, COLOR_SOFT, "OK 翻面并发音");
    } else {
        lv_obj_t *word = label(card, 0, 16, &lv_font_montserrat_20,
                               COLOR_INK, entry->w);
        lv_obj_set_size(word, 208, 30);
        lv_obj_set_style_text_align(word, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_t *zh = label(card, 20, 52, &app_font_16_fb, COLOR_INK, entry->zh);
        lv_obj_set_width(zh, 168);
        lv_label_set_long_mode(zh, LV_LABEL_LONG_WRAP);
        lv_obj_t *en = label(card, 20, 86, &app_font_16_fb, COLOR_SOFT, entry->en);
        lv_obj_set_width(en, 168);
        lv_label_set_long_mode(en, LV_LABEL_LONG_WRAP);
        char example[256];
        snprintf(example, sizeof(example), "%s  %s", entry->ex, entry->exz);
        lv_obj_t *ex = label(card, 20, 150, &app_font_16_fb, COLOR_INK, example);
        lv_obj_set_width(ex, 168);
        lv_label_set_long_mode(ex, LV_LABEL_LONG_WRAP);
    }

    for (int i = 0; i < 10; i++) {
        rect(scr, 92 + i * 8, 288, 5, 5,
             i == (int)s_app.study_pos ? 0xB45309 : 0xC9BFA4);
    }
    lv_screen_load(scr);
}

static void render_study_done(void)
{
    lv_obj_t *scr = screen_new();
    header_add(scr, "每日学习");
    label(scr, 34, 110, &app_font_20_fb, COLOR_INK, "10 词完成");
    char sub[64];
    snprintf(sub, sizeof(sub), "已学习 %u 词", (unsigned)s_model.session_studied);
    label(scr, 34, 142, &app_font_16_fb, COLOR_SOFT, sub);
    lv_obj_t *row = row_box(scr, 190, 42, COLOR_SELECT, true);
    label(row, 58, 10, &app_font_16_fb, COLOR_INK, "OK 返回首页");
    lv_screen_load(scr);
}

static void render_quiz(void)
{
    lv_obj_t *scr = screen_new();
    header_add(scr, "单词自测");
    if (s_app.quiz_pos >= QUIZ_ITEM_COUNT) {
        uint16_t percent = s_model.quiz_answered
                               ? s_model.quiz_correct * 100U / s_model.quiz_answered
                               : 0;
        char result[32];
        snprintf(result, sizeof(result), "%u%%", (unsigned)percent);
        label(scr, 74, 110, &app_font_20_fb, COLOR_INK, result);
        char sub[64];
        snprintf(sub, sizeof(sub), "10 题 · 答对 %u", (unsigned)s_model.quiz_correct);
        label(scr, 34, 142, &app_font_16_fb, COLOR_SOFT, sub);
        lv_obj_t *row = row_box(scr, 190, 42, COLOR_SELECT, true);
        label(row, 58, 10, &app_font_16_fb, COLOR_INK, "OK 返回首页");
        lv_screen_load(scr);
        return;
    }

    const word_quiz_item_t *item = &s_app.quiz_items[s_app.quiz_pos];
    const word_entry_t *prompt = &WORD_LIST[item->correct];
    char head[32];
    snprintf(head, sizeof(head), "%u / 10", (unsigned)(s_app.quiz_pos + 1));
    label(scr, 12, 44, &app_font_16_fb, COLOR_SOFT, head);

    lv_obj_t *prompt_box = rect(scr, 10, 56, 220, 44, COLOR_WHITE);
    lv_obj_set_style_radius(prompt_box, 8, 0);
    lv_obj_set_style_border_width(prompt_box, 1, 0);
    lv_obj_set_style_border_color(prompt_box, lv_color_hex(COLOR_LINE), 0);
    lv_obj_t *prompt_label = label(prompt_box, 0, 5, &app_font_16_fb,
                                   COLOR_INK, prompt->zh);
    lv_obj_set_size(prompt_label, 220, 34);
    lv_obj_set_style_text_align(prompt_label, LV_TEXT_ALIGN_CENTER, 0);

    for (int i = 0; i < QUIZ_OPTIONS; i++) {
        int y = 108 + i * 52;
        uint32_t bg = COLOR_WHITE;
        bool selected = false;
        if (s_app.quiz_answered) {
            if (i == (int)item->answer) bg = 0xDCFCE7;
            else if (i == (int)s_app.quiz_chosen) bg = 0xFEE2E2;
            else bg = 0xF1EBDE;
        } else if (i == (int)s_app.quiz_chosen) {
            selected = true;
        }
        lv_obj_t *row = row_box(scr, y, 46, bg, selected);
        label(row, 14, 11, &app_font_20_fb, COLOR_INK,
              WORD_LIST[item->options[i]].w);
    }

    if (s_app.quiz_answered) {
        bool correct = s_app.quiz_chosen == item->answer;
        const char *text = correct ? "回答正确" : "OK 下一题";
        label(scr, 12, 280, &app_font_16_fb,
              correct ? 0x166534 : 0xB91C1C, text);
    }
    lv_screen_load(scr);
}

static void render_collection(void)
{
    lv_obj_t *scr = screen_new();
    header_add(scr, "生词本");
    if (s_collection_count == 0) {
        label(scr, 54, 130, &app_font_16_fb, COLOR_SOFT, "还没有收藏单词");
        label(scr, 30, 158, &app_font_16_fb, COLOR_SOFT, "详情页双击 OK 收藏");
        lv_screen_load(scr);
        return;
    }
    char head[32];
    snprintf(head, sizeof(head), "%u 个", (unsigned)s_collection_count);
    label(scr, 12, 44, &app_font_16_fb, COLOR_SOFT, head);
    for (int i = 0; i < 6; i++) {
        uint16_t position = s_app.list_offset + (uint16_t)i;
        if (position >= s_collection_count) break;
        int y = 62 + i * 38;
        bool selected = position == s_app.list_selected;
        const word_entry_t *entry = &WORD_LIST[s_collection[position]];
        lv_obj_t *row = row_box(scr, y, 34, selected ? 0xFEF3C7 : COLOR_WHITE,
                                selected);
        label(row, 38, 8, &lv_font_montserrat_20, COLOR_INK, entry->w);
        lv_obj_t *zh = label(row, 132, 10, &app_font_16_fb, COLOR_SOFT, entry->zh);
        lv_obj_set_width(zh, 80);
        lv_label_set_long_mode(zh, LV_LABEL_LONG_DOT);
    }
    lv_screen_load(scr);
}

static void render_stats(void)
{
    lv_obj_t *scr = screen_new();
    header_add(scr, "学习统计");
    uint16_t mastered = 0, learning = 0, starred = 0;
    word_model_stats(&s_model, &mastered, &learning, &starred);
    char text[32];
    const char *names[5] = { "已掌握", "学习中", "生词本", "本次学习", "自测正确率" };
    for (int i = 0; i < 5; i++) {
        int y = 50 + i * 47;
        lv_obj_t *row = row_box(scr, y, 40, COLOR_WHITE, false);
        label(row, 14, 10, &app_font_16_fb, COLOR_INK, names[i]);
        switch (i) {
        case 0: snprintf(text, sizeof(text), "%u", (unsigned)mastered); break;
        case 1: snprintf(text, sizeof(text), "%u", (unsigned)learning); break;
        case 2: snprintf(text, sizeof(text), "%u", (unsigned)starred); break;
        case 3: snprintf(text, sizeof(text), "%u", (unsigned)s_model.session_studied); break;
        default:
            snprintf(text, sizeof(text), "%u%%",
                     (unsigned)(s_model.quiz_answered
                                    ? s_model.quiz_correct * 100U
                                          / s_model.quiz_answered
                                    : 0));
            break;
        }
        label(row, 150, 10, &app_font_20_fb, COLOR_INK, text);
    }
    lv_screen_load(scr);
}

static void render_screen(void)
{
    switch (s_app.screen) {
    case SCREEN_HOME: render_home(); break;
    case SCREEN_CATEGORY: render_categories(); break;
    case SCREEN_LIST: render_word_list(); break;
    case SCREEN_DETAIL: render_detail(); break;
    case SCREEN_STUDY: render_study(); break;
    case SCREEN_STUDY_DONE: render_study_done(); break;
    case SCREEN_QUIZ: render_quiz(); break;
    case SCREEN_COLLECTION: render_collection(); break;
    case SCREEN_STATS: render_stats(); break;
    default: render_home(); break;
    }
}

static void play_index(uint16_t word_index)
{
    if (s_audio_ok) audio_player_play(word_index);
}

static void start_study(void)
{
    for (int i = 0; i < QUIZ_ITEM_COUNT; i++) {
        s_app.study_deck[i] = (uint8_t)i;
    }
    s_app.study_pos = 0;
    s_app.study_revealed = false;
    s_app.screen = SCREEN_STUDY;
}

static void start_quiz(void)
{
    uint32_t seed = esp_random();
    word_quiz_build_items(WORD_COUNT, seed, s_app.quiz_items);
    s_app.quiz_pos = 0;
    s_app.quiz_answered = false;
    s_app.quiz_chosen = 0;
    s_app.screen = SCREEN_QUIZ;
    s_model.quiz_answered = 0;
    s_model.quiz_correct = 0;
}

static void back_screen(void)
{
    switch (s_app.screen) {
    case SCREEN_HOME:
        break;
    case SCREEN_CATEGORY:
    case SCREEN_STUDY:
    case SCREEN_STUDY_DONE:
    case SCREEN_QUIZ:
    case SCREEN_COLLECTION:
    case SCREEN_STATS:
        s_app.screen = SCREEN_HOME;
        break;
    case SCREEN_LIST:
        if (s_app.list_from_all) s_app.screen = SCREEN_HOME;
        else s_app.screen = SCREEN_CATEGORY;
        break;
    case SCREEN_DETAIL:
        s_app.screen = SCREEN_LIST;
        break;
    default:
        s_app.screen = SCREEN_HOME;
        break;
    }
}

static void handle_up(void)
{
    switch (s_app.screen) {
    case SCREEN_HOME:
        s_app.home_index = (uint8_t)((s_app.home_index + 5) % 6);
        break;
    case SCREEN_CATEGORY:
        s_app.category_index = (uint8_t)((s_app.category_index + 5) % 6);
        break;
    case SCREEN_LIST:
    case SCREEN_COLLECTION: {
        uint16_t count = list_count();
        if (count == 0) break;
        s_app.list_selected = (s_app.list_selected + count - 1) % count;
        if (s_app.list_offset > s_app.list_selected &&
            s_app.list_offset - s_app.list_selected > 5) {
            s_app.list_offset = s_app.list_selected;
        }
        break;
    }
    case SCREEN_DETAIL:
        {
            uint16_t count = list_count();
            if (count == 0) break;
            s_app.detail_index = (s_app.detail_index + count - 1) % count;
        }
        break;
    case SCREEN_STUDY:
        if (s_app.study_pos > 0) {
            s_app.study_pos--;
            s_app.study_revealed = false;
        }
        break;
    case SCREEN_QUIZ:
        if (!s_app.quiz_answered) {
            s_app.quiz_chosen = (uint8_t)((s_app.quiz_chosen + 2) % QUIZ_OPTIONS);
            return;
        }
        play_index(s_app.quiz_items[s_app.quiz_pos].correct);
        break;
    default:
        break;
    }
}

static void handle_down(void)
{
    switch (s_app.screen) {
    case SCREEN_HOME:
        s_app.home_index = (uint8_t)((s_app.home_index + 1) % 6);
        break;
    case SCREEN_CATEGORY:
        s_app.category_index = (uint8_t)((s_app.category_index + 1) % 6);
        break;
    case SCREEN_LIST:
    case SCREEN_COLLECTION: {
        uint16_t count = list_count();
        if (count == 0) break;
        s_app.list_selected = (s_app.list_selected + 1) % count;
        if (s_app.list_selected >= s_app.list_offset + 6) {
            s_app.list_offset = s_app.list_selected - 5;
        }
        break;
    }
    case SCREEN_DETAIL:
        {
            uint16_t count = list_count();
            if (count == 0) break;
            s_app.detail_index = (s_app.detail_index + 1) % count;
        }
        break;
    case SCREEN_STUDY:
        if (s_app.study_pos + 1 < QUIZ_ITEM_COUNT) {
            s_app.study_pos++;
            s_app.study_revealed = false;
        } else {
            word_store_request_save(&s_model);
            s_app.screen = SCREEN_STUDY_DONE;
        }
        break;
    case SCREEN_QUIZ:
        if (!s_app.quiz_answered) {
            s_app.quiz_chosen = (uint8_t)((s_app.quiz_chosen + 1) % QUIZ_OPTIONS);
        } else {
            play_index(s_app.quiz_items[s_app.quiz_pos].correct);
        }
        break;
    default:
        break;
    }
}

static void handle_ok(void)
{
    switch (s_app.screen) {
    case SCREEN_HOME:
        switch (s_app.home_index) {
        case 0:
            s_app.screen = SCREEN_CATEGORY;
            s_app.category_index = 0;
            break;
        case 1:
            start_study();
            break;
        case 2:
            start_quiz();
            break;
        case 3:
            rebuild_collection();
            s_app.screen = SCREEN_COLLECTION;
            s_app.list_selected = 0;
            s_app.list_offset = 0;
            break;
        case 4:
            s_app.screen = SCREEN_STATS;
            break;
        default:
            s_app.list_from_all = true;
            s_app.list_category = 0;
            s_app.screen = SCREEN_LIST;
            s_app.list_selected = 0;
            s_app.list_offset = 0;
            break;
        }
        break;
    case SCREEN_CATEGORY:
        s_app.list_from_all = s_app.category_index == 0;
        s_app.list_category = s_app.category_index;
        s_app.screen = SCREEN_LIST;
        s_app.list_selected = 0;
        s_app.list_offset = 0;
        break;
    case SCREEN_LIST:
    case SCREEN_COLLECTION: {
        if (list_count() == 0) break;
        s_app.detail_index = s_app.list_selected;
        s_app.screen = SCREEN_DETAIL;
        break;
    }
    case SCREEN_DETAIL: {
        uint16_t word_index = list_get_index(s_app.detail_index);
        play_index(word_index);
        break;
    }
    case SCREEN_STUDY: {
        uint16_t word_index = s_app.study_deck[s_app.study_pos];
        if (!s_app.study_revealed) {
            s_app.study_revealed = true;
            s_model.status[word_index] = WORD_STATUS_LEARNING;
            s_model.session_studied++;
        }
        play_index(word_index);
        break;
    }
    case SCREEN_QUIZ:
        if (s_app.quiz_pos >= QUIZ_ITEM_COUNT) {
            s_app.screen = SCREEN_HOME;
            break;
        }
        if (!s_app.quiz_answered) {
            const word_quiz_item_t *item = &s_app.quiz_items[s_app.quiz_pos];
            s_model.quiz_answered++;
            if (s_app.quiz_chosen == item->answer) {
                s_model.quiz_correct++;
                s_model.status[item->correct] = WORD_STATUS_MASTERED;
            } else {
                s_model.status[item->correct] = WORD_STATUS_LEARNING;
            }
            s_app.quiz_answered = true;
            play_index(item->correct);
            word_store_request_save(&s_model);
        } else if (s_app.quiz_pos + 1 < QUIZ_ITEM_COUNT) {
            s_app.quiz_pos++;
            s_app.quiz_answered = false;
            s_app.quiz_chosen = 0;
        } else {
            s_app.quiz_pos = QUIZ_ITEM_COUNT;
            word_store_request_save(&s_model);
        }
        break;
    case SCREEN_STUDY_DONE:
    case SCREEN_STATS:
        s_app.screen = SCREEN_HOME;
        break;
    default:
        break;
    }
}

static void handle_double(bsp_btn_t btn)
{
    if (s_app.screen != SCREEN_DETAIL) return;
    if (btn != BSP_BTN_OK) return;
    uint16_t word_index = list_get_index(s_app.detail_index);
    word_model_toggle_star(&s_model, word_index);
    rebuild_collection();
    word_store_request_save(&s_model);
}

void word_app_init(void)
{
    app_fonts_init();
    memset(&s_app, 0, sizeof(s_app));
    memset(&s_model, 0, sizeof(s_model));
    word_store_init(&s_model);
    s_audio_ok = audio_player_start();
    if (!s_audio_ok) ESP_LOGW(TAG, "word pronunciation disabled");
    render_home();
}

void word_app_handle_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (!bsp_lvgl_lock(500)) return;
    if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        back_screen();
    } else if (ev == BSP_BTN_DOUBLE) {
        if (btn == BSP_BTN_UP) handle_up();
        else if (btn == BSP_BTN_DOWN) handle_down();
        else handle_double(btn);
    } else if (ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_UP) handle_up();
        else if (btn == BSP_BTN_DOWN) handle_down();
        else if (btn == BSP_BTN_OK) handle_ok();
    }
    render_screen();
    bsp_lvgl_unlock();
}
