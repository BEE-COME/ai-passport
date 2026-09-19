#include <assert.h>
#include <string.h>

#include "word_model.h"

int main(void)
{
    word_model_t model;
    word_model_defaults(&model);

    assert(word_model_status(&model, 3) == WORD_STATUS_NEW);
    word_model_set_status(&model, 3, WORD_STATUS_LEARNING);
    assert(word_model_status(&model, 3) == WORD_STATUS_LEARNING);

    assert(!word_model_is_starred(&model, 7));
    word_model_toggle_star(&model, 7);
    assert(word_model_is_starred(&model, 7));
    word_model_toggle_star(&model, 7);
    assert(!word_model_is_starred(&model, 7));

    word_model_set_status(&model, 0, WORD_STATUS_MASTERED);
    word_model_toggle_star(&model, 9);
    uint16_t mastered = 0, learning = 0, starred = 0;
    word_model_stats(&model, &mastered, &learning, &starred);
    assert(mastered == 1);
    assert(learning == 1);
    assert(starred == 1);
    return 0;
}
