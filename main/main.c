// FoloToy AI Passport 单词学习本：BSP 初始化 + 三键输入分发。
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "word_app.h"

static const char *TAG = "main";

#define INPUT_QUEUE_DEPTH 8

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue;
static volatile bool s_input_ready;

static void input_task(void *arg)
{
    (void)arg;
    input_event_t input;
    while (xQueueReceive(s_input_queue, &input, portMAX_DELAY) == pdTRUE) {
        word_app_handle_key(input.btn, input.event);
    }
    vTaskDelete(NULL);
}

static esp_err_t input_dispatch_init(void)
{
    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue) return ESP_ERR_NO_MEM;
    TaskHandle_t task = NULL;
    if (xTaskCreate(input_task, "word_input", 16384, NULL, 5, &task) != pdPASS) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

// 按键回调运行在 BSP 的共享 esp_timer 任务里，只能入队后立即返回。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (!s_input_ready || !s_input_queue) return;
    input_event_t input = { .btn = btn, .event = ev };
    (void)xQueueSend(s_input_queue, &input, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "FoloToy AI Passport 单词学习本启动");
    bsp_i2c_init();
    bsp_i2c_scan();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败,检查 SPI(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    bool buttons_ok = false;
    if (input_dispatch_init() == ESP_OK) {
        buttons_ok = bsp_button_init(on_key, NULL) == ESP_OK;
    }
    bool battery_ok = bsp_battery_init() == ESP_OK;
    (void)battery_ok;

    if (bsp_lvgl_lock(1000)) {
        word_app_init();
        bsp_lvgl_unlock();
    }

    s_input_ready = true;
    ESP_LOGI(TAG, "就绪:Display=1 Button=%d Battery=%d", buttons_ok, battery_ok);
}
