#pragma once

#include <stdbool.h>

#include "word_model.h"

bool word_store_init(word_model_t *model);
void word_store_request_save(const word_model_t *model);
