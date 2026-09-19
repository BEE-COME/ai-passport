#include <assert.h>
#include <string.h>

#include "word_data.h"

int main(void)
{
    assert(WORD_COUNT == 850);
    assert(WORD_CATEGORY_COUNT == 5);
    assert(WORD_CATEGORY_COUNTS[0] == 100);
    assert(WORD_CATEGORY_COUNTS[1] == 400);
    assert(WORD_CATEGORY_COUNTS[2] == 200);
    assert(WORD_CATEGORY_COUNTS[3] == 100);
    assert(WORD_CATEGORY_COUNTS[4] == 50);

    for (uint16_t i = 0; i < WORD_COUNT; i++) {
        const word_entry_t *entry = &WORD_LIST[i];
        assert(entry->w && entry->w[0]);
        assert(entry->zh && entry->zh[0]);
        assert(entry->en && entry->en[0]);
        assert(entry->ex && entry->ex[0]);
        assert(entry->exz && entry->exz[0]);
        assert(entry->core && entry->core[0]);
        assert(entry->cat < WORD_CATEGORY_COUNT);
        for (int s = 0; s < 3; s++) {
            if (entry->syn[s]) assert(entry->syn[s][0]);
        }
    }

    for (uint16_t i = 0; i < WORD_COUNT; i++) {
        for (uint16_t j = i + 1; j < WORD_COUNT; j++) {
            assert(strcmp(WORD_LIST[i].w, WORD_LIST[j].w) != 0);
        }
    }
    return 0;
}
