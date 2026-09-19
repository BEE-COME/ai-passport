idf_component_register(
    SRCS "main.c"
         "app_fonts.c"
         "word_data.c"
         "audio_index.c"
         "word_model.c"
         "word_quiz.c"
         "word_store.c"
         "audio_player.c"
         "word_app.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_hw_support esp_timer nvs_flash
)

target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/app_font_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/app_font_20.c"
)

target_add_binary_data(${COMPONENT_LIB}
    "${CMAKE_CURRENT_LIST_DIR}/../assets/audio/ogden_words.adpcm"
    BINARY
    RENAME_TO ogden_words_blob
)
