#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "word_data.h"

typedef enum {
    WORD_STATUS_NEW = 0,
    WORD_STATUS_LEARNING = 1,
    WORD_STATUS_MASTERED = 2,
} word_status_t;

typedef struct {
    uint8_t status[WORD_COUNT];
    uint8_t starred[WORD_COUNT];
    uint16_t session_studied;
    uint16_t quiz_answered;
    uint16_t quiz_correct;
} word_model_t;

void word_model_defaults(word_model_t *model);
uint8_t word_model_status(const word_model_t *model, uint16_t index);
void word_model_set_status(word_model_t *model, uint16_t index, uint8_t status);
bool word_model_is_starred(const word_model_t *model, uint16_t index);
void word_model_toggle_star(word_model_t *model, uint16_t index);
void word_model_stats(const word_model_t *model, uint16_t *mastered,
                      uint16_t *learning, uint16_t *starred);
