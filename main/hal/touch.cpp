#include "hal/hal.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "hal:touch";

static hal::touch_handler_t touch_handler_ = nullptr;
static TaskHandle_t touch_task_ = nullptr;
static bool touch_running_ = false;

static void touch_poll_task(void *arg)
{
    lv_point_t last_point = {0, 0};
    bool was_pressed = false;

    while (touch_running_) {
        if (!touch_handler_) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        hal::display_lock();
        lv_indev_t *indev = lv_indev_get_next(NULL);
        if (indev) {
            lv_point_t point;
            lv_indev_state_t state = lv_indev_get_state(indev);
            lv_indev_get_point(indev, &point);

            bool pressed = (state == LV_INDEV_STATE_PRESSED);

            if (pressed && !was_pressed) {
                // Press
                touch_handler_(point.x, point.y, 0);
            } else if (pressed && was_pressed &&
                       (point.x != last_point.x || point.y != last_point.y)) {
                // Move
                touch_handler_(point.x, point.y, 2);
            } else if (!pressed && was_pressed) {
                // Release
                touch_handler_(last_point.x, last_point.y, 1);
            }

            was_pressed = pressed;
            last_point = point;
        }
        hal::display_unlock();

        vTaskDelay(pdMS_TO_TICKS(50)); // 20Hz polling
    }

    touch_task_ = nullptr;
    vTaskDelete(NULL);
}

void hal::touch_init()
{
    ESP_LOGI(TAG, "Touch initialized (LVGL indev)");
}

void hal::touch_enable_events(touch_handler_t handler)
{
    touch_handler_ = handler;
    if (!touch_running_) {
        touch_running_ = true;
        xTaskCreate(touch_poll_task, "touch_poll", 4096, NULL, 4, &touch_task_);
        ESP_LOGI(TAG, "Touch events enabled");
    }
}

void hal::touch_disable_events()
{
    touch_running_ = false;
    touch_handler_ = nullptr;
    ESP_LOGI(TAG, "Touch events disabled");
}
