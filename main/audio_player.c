// Offline ADPCM word playback through the BSP ES8311 audio path.
#include "audio_player.h"

#include "audio_index.h"
#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "word_audio";

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_CHUNK 256
#define AUDIO_STOP_TIMEOUT_MS 2000

static const int16_t IMA_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767,
};

static const int8_t IMA_INDEX[16] = { -1, -1, -1, -1, 2, 4, 6, 8,
                                      -1, -1, -1, -1, 2, 4, 6, 8 };

extern const uint8_t _binary_ogden_words_blob_start[];
extern const uint8_t _binary_ogden_words_blob_end[];

static TaskHandle_t s_task;
static SemaphoreHandle_t s_stopped;
static volatile bool s_cancel;
static volatile bool s_ready;

static uint8_t read_u8(const uint8_t *data, uint32_t *offset)
{
    return data[(*offset)++];
}

static uint16_t read_u16(const uint8_t *data, uint32_t *offset)
{
    uint16_t value = data[*offset] | ((uint16_t)data[*offset + 1] << 8);
    *offset += 2;
    return value;
}

static uint32_t read_u32(const uint8_t *data, uint32_t *offset)
{
    uint32_t value = (uint32_t)data[*offset]
                     | ((uint32_t)data[*offset + 1] << 8)
                     | ((uint32_t)data[*offset + 2] << 16)
                     | ((uint32_t)data[*offset + 3] << 24);
    *offset += 4;
    return value;
}

static int16_t decode_sample(uint8_t nibble, int16_t *predictor,
                             uint8_t *step_index)
{
    int step = IMA_STEP[*step_index];
    int delta = step >> 3;
    if (nibble & 4) delta += step;
    if (nibble & 2) delta += step >> 1;
    if (nibble & 1) delta += step >> 2;
    int32_t value = (int32_t)*predictor + ((nibble & 8) ? -delta : delta);
    if (value > 32767) value = 32767;
    if (value < -32768) value = -32768;
    *predictor = (int16_t)value;
    int index = (int)*step_index + IMA_INDEX[nibble & 7];
    if (index < 0) index = 0;
    if (index > 88) index = 88;
    *step_index = (uint8_t)index;
    return *predictor;
}

static void play_word(uint16_t word_index)
{
    if (word_index >= WORD_COUNT ||
        WORD_AUDIO_OFFSET[word_index] + WORD_AUDIO_LEN[word_index] >
            (uint32_t)(_binary_ogden_words_blob_end - _binary_ogden_words_blob_start)) {
        ESP_LOGW(TAG, "invalid audio index %u", (unsigned)word_index);
        return;
    }

    const uint8_t *blob = _binary_ogden_words_blob_start
                          + WORD_AUDIO_OFFSET[word_index];
    uint32_t offset = 0;
    uint32_t samples = read_u32(blob, &offset);
    int16_t predictor = (int16_t)read_u16(blob, &offset);
    uint8_t step_index = read_u8(blob, &offset);
    uint8_t reserved = read_u8(blob, &offset);
    (void)reserved;

    int16_t buffer[AUDIO_CHUNK];

    uint32_t decoded = 0;
    while (decoded < samples && !s_cancel) {
        size_t count = samples - decoded < AUDIO_CHUNK ? samples - decoded : AUDIO_CHUNK;
        size_t pairs = (count + 1) / 2;
        for (size_t i = 0; i < pairs; i++) {
            uint8_t byte = blob[offset++];
            uint8_t hi = (byte >> 4) & 0x0F;
            uint8_t lo = byte & 0x0F;
            buffer[i * 2] = decode_sample(hi, &predictor, &step_index);
            if (i * 2 + 1 < count) {
                buffer[i * 2 + 1] = decode_sample(lo, &predictor, &step_index);
            }
        }
        if (count == 0) break;
        bsp_audio_write(buffer, count * sizeof(int16_t));
        decoded += count;
    }
}

static void audio_task(void *arg)
{
    (void)arg;
    uint32_t command = 0;
    if (bsp_audio_set_format(AUDIO_SAMPLE_RATE, 16, 1) != ESP_OK) {
        ESP_LOGE(TAG, "audio format setup failed");
        s_ready = false;
        if (s_stopped) xSemaphoreGive(s_stopped);
        vTaskDelete(NULL);
        return;
    }
    bsp_audio_set_volume(75);

    for (;;) {
        if (xTaskNotifyWait(0, UINT32_MAX, &command, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (command == 0 || command == UINT32_MAX) break;
        uint16_t index = (uint16_t)(command - 1);
        if (index < WORD_COUNT) play_word(index);
    }
    s_ready = false;
    if (s_stopped) xSemaphoreGive(s_stopped);
    s_task = NULL;
    vTaskDelete(NULL);
}

bool audio_player_start(void)
{
    if (s_ready) return true;
    if (bsp_audio_init() != ESP_OK) {
        ESP_LOGW(TAG, "audio unavailable; pronunciation disabled");
        return false;
    }
    s_stopped = xSemaphoreCreateBinary();
    if (!s_stopped) return false;
    s_cancel = false;
    if (xTaskCreate(audio_task, "word_audio", 16384, NULL, 4, &s_task) != pdPASS) {
        vSemaphoreDelete(s_stopped);
        s_stopped = NULL;
        return false;
    }
    s_ready = true;
    return true;
}

void audio_player_play(uint16_t word_index)
{
    if (!s_ready || !s_task || word_index >= WORD_COUNT) return;
    s_cancel = false;
    xTaskNotify(s_task, (uint32_t)word_index + 1, eSetValueWithOverwrite);
}

void audio_player_stop(void)
{
    TaskHandle_t task = s_task;
    if (!task) return;
    s_cancel = true;
    xTaskNotify(task, UINT32_MAX, eSetValueWithOverwrite);
    if (s_stopped &&
        xSemaphoreTake(s_stopped, pdMS_TO_TICKS(AUDIO_STOP_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "audio stop timed out");
        return;
    }
    s_task = NULL;
    vSemaphoreDelete(s_stopped);
    s_stopped = NULL;
    s_ready = false;
}
