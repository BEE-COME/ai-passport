#include "word_model.h"

#include <string.h>

void word_model_defaults(word_model_t *model)
{
    if (!model) return;
    memset(model, 0, sizeof(*model));
}

uint8_t word_model_status(const word_model_t *model, uint16_t index)
{
    if (!model || index >= WORD_COUNT) return WORD_STATUS_NEW;
    return model->status[index];
}

void word_model_set_status(word_model_t *model, uint16_t index, uint8_t status)
{
    if (!model || index >= WORD_COUNT || status > WORD_STATUS_MASTERED) return;
    model->status[index] = status;
}

bool word_model_is_starred(const word_model_t *model, uint16_t index)
{
    if (!model || index >= WORD_COUNT) return false;
    return model->starred[index] != 0;
}

void word_model_toggle_star(word_model_t *model, uint16_t index)
{
    if (!model || index >= WORD_COUNT) return;
    model->starred[index] = model->starred[index] ? 0 : 1;
}

void word_model_stats(const word_model_t *model, uint16_t *mastered,
                      uint16_t *learning, uint16_t *starred)
{
    uint16_t m = 0, l = 0, s = 0;
    if (model) {
        for (uint16_t i = 0; i < WORD_COUNT; i++) {
            if (model->status[i] == WORD_STATUS_MASTERED) m++;
            else if (model->status[i] == WORD_STATUS_LEARNING) l++;
            if (model->starred[i]) s++;
        }
    }
    if (mastered) *mastered = m;
    if (learning) *learning = l;
    if (starred) *starred = s;
}
