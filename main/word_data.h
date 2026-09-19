#pragma once

#include <stdint.h>

#define WORD_COUNT 850
#define WORD_CATEGORY_COUNT 5

typedef struct {
    const char *w;
    const char *zh;
    const char *en;
    const char *ex;
    const char *exz;
    const char *core;
    const char *syn[3];
    uint8_t cat;
} word_entry_t;

extern const word_entry_t WORD_LIST[WORD_COUNT];
extern const char *const WORD_CATEGORY_ZH[WORD_CATEGORY_COUNT];
extern const uint32_t WORD_CATEGORY_COUNTS[WORD_CATEGORY_COUNT];
