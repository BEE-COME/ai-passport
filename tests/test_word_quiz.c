#include <assert.h>
#include <string.h>

#include "word_quiz.h"

int main(void)
{
    uint32_t rng = 0x12345678U;
    word_quiz_item_t item;
    word_quiz_make_item(10, 850, &rng, &item);
    assert(item.correct == 10);
    assert(item.options[item.answer] == item.correct);
    assert(item.options[0] != item.options[1]);
    assert(item.options[0] != item.options[2]);
    assert(item.options[1] != item.options[2]);

    word_quiz_item_t items[QUIZ_ITEM_COUNT];
    assert(word_quiz_build_items(850, 42, items));
    for (int i = 0; i < QUIZ_ITEM_COUNT; i++) {
        assert(items[i].options[items[i].answer] == items[i].correct);
        assert(items[i].options[0] != items[i].options[1]);
        assert(items[i].options[0] != items[i].options[2]);
        assert(items[i].options[1] != items[i].options[2]);
        for (int j = i + 1; j < QUIZ_ITEM_COUNT; j++) {
            assert(items[i].correct != items[j].correct);
        }
    }
    return 0;
}
