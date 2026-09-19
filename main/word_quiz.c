#include "word_quiz.h"

#include <string.h>

uint32_t word_quiz_rand(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static uint16_t random_index(uint32_t *rng, uint16_t limit)
{
    return (uint16_t)(word_quiz_rand(rng) % limit);
}

void word_quiz_make_item(uint16_t correct, uint16_t word_count, uint32_t *rng,
                         word_quiz_item_t *item)
{
    if (!item || word_count < QUIZ_OPTIONS) return;
    uint16_t options[QUIZ_OPTIONS] = { 0, 0, 0 };
    options[0] = correct;
    for (int i = 1; i < QUIZ_OPTIONS; i++) {
        uint16_t candidate;
        bool duplicate;
        do {
            candidate = random_index(rng, word_count);
            duplicate = false;
            for (int j = 0; j < i; j++) {
                if (options[j] == candidate) {
                    duplicate = true;
                    break;
                }
            }
        } while (duplicate);
        options[i] = candidate;
    }

    uint8_t answer = (uint8_t)random_index(rng, QUIZ_OPTIONS);
    item->correct = correct;
    item->answer = answer;
    uint16_t distractors[QUIZ_OPTIONS - 1] = { options[1], options[2] };
    int used = 0;
    for (int i = 0; i < QUIZ_OPTIONS; i++) {
        if (i == answer) item->options[i] = correct;
        else item->options[i] = distractors[used++];
    }
}

bool word_quiz_build_items(uint16_t word_count, uint32_t seed,
                           word_quiz_item_t items[QUIZ_ITEM_COUNT])
{
    if (!items || word_count < QUIZ_OPTIONS) return false;
    uint32_t rng = seed ? seed : 1;
    for (int i = 0; i < QUIZ_ITEM_COUNT; i++) {
        uint16_t correct;
        bool duplicate;
        do {
            correct = random_index(&rng, word_count);
            duplicate = false;
            for (int j = 0; j < i; j++) {
                if (items[j].correct == correct) {
                    duplicate = true;
                    break;
                }
            }
        } while (duplicate);
        word_quiz_make_item(correct, word_count, &rng, &items[i]);
    }
    return true;
}
