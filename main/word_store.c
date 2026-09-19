#include "word_store.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define STORE_MAGIC 0x574F5244U
#define STORE_VERSION 1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t crc;
    word_model_t model;
} word_store_blob_t;

static const char *TAG = "word_store";
static nvs_handle_t s_nvs;
static QueueHandle_t s_queue;
static bool s_ready;

static uint32_t crc32_bytes(const void *data, size_t len)
{
    const uint8_t *bytes = data;
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)-(int32_t)(crc & 1));
        }
    }
    return ~crc;
}

static word_store_blob_t blob_from_model(const word_model_t *model)
{
    word_store_blob_t blob = { 0 };
    blob.magic = STORE_MAGIC;
    blob.version = STORE_VERSION;
    blob.size = sizeof(blob);
    blob.model = *model;
    blob.crc = crc32_bytes(&blob, offsetof(word_store_blob_t, crc));
    return blob;
}

static void load_blob(word_model_t *model)
{
    word_store_blob_t blob;
    size_t size = sizeof(blob);
    esp_err_t err = nvs_get_blob(s_nvs, "state", &blob, &size);
    if (err == ESP_OK && size == sizeof(blob) &&
        blob.magic == STORE_MAGIC && blob.version == STORE_VERSION &&
        blob.size == sizeof(blob) &&
        blob.crc == crc32_bytes(&blob, offsetof(word_store_blob_t, crc))) {
        *model = blob.model;
        ESP_LOGI(TAG, "restored learning state");
    } else {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "stored state invalid or unreadable: %s",
                     esp_err_to_name(err));
        }
        word_model_defaults(model);
    }
}

static void save_task(void *arg)
{
    (void)arg;
    word_store_blob_t blob;
    while (xQueueReceive(s_queue, &blob, portMAX_DELAY) == pdTRUE) {
        esp_err_t err = nvs_set_blob(s_nvs, "state", &blob, sizeof(blob));
        if (err == ESP_OK) err = nvs_commit(s_nvs);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS save failed: %s", esp_err_to_name(err));
        }
    }
    vTaskDelete(NULL);
}

bool word_store_init(word_model_t *model)
{
    if (!model) return false;
    if (s_ready) return true;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs recovery: %s", esp_err_to_name(err));
        err = nvs_flash_erase();
        if (err == ESP_OK) err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        word_model_defaults(model);
        return false;
    }
    err = nvs_open("wordapp", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        word_model_defaults(model);
        return false;
    }

    load_blob(model);
    s_queue = xQueueCreate(1, sizeof(word_store_blob_t));
    if (!s_queue || xTaskCreate(save_task, "word_save", 8192, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "persistence worker creation failed");
        word_model_defaults(model);
        return false;
    }
    s_ready = true;
    return true;
}

void word_store_request_save(const word_model_t *model)
{
    if (!s_ready || !s_queue || !model) return;
    word_store_blob_t blob = blob_from_model(model);
    xQueueOverwrite(s_queue, &blob);
}
