#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define QUIZ_OPTIONS 3
#define QUIZ_ITEM_COUNT 10

typedef struct {
    uint16_t correct;
    uint16_t options[QUIZ_OPTIONS];
    uint8_t answer;
} word_quiz_item_t;

uint32_t word_quiz_rand(uint32_t *state);
void word_quiz_make_item(uint16_t correct, uint16_t word_count, uint32_t *rng,
                         word_quiz_item_t *item);
bool word_quiz_build_items(uint16_t word_count, uint32_t seed,
                           word_quiz_item_t items[QUIZ_ITEM_COUNT]);
