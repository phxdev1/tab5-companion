#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "hal/hal.h"
#include "cmd/cmd.h"
#include "ui/ui.h"
#include "esp_log.h"

static const char *TAG = "main";

extern "C" void app_main(void)
{
    // Display — backlight stays off until we're ready
    hal::display_init();

    // Dark screen + standby UI (BLE pulsing icon)
    ui::standby_init();

    // Backlight on low
    vTaskDelay(pdMS_TO_TICKS(100));
    hal::display_set_brightness(20);

    // BLE — power C6, init NimBLE, start advertising
    hal::ble_init();

    // Command dispatch — wires BLE commands to handlers
    cmd::init();

    ESP_LOGI(TAG, "Tab5 ready — waiting for agent");

    // Monitor BLE connection state
    bool was_connected = false;
    while (true) {
        bool connected = hal::ble_is_connected();
        if (connected != was_connected) {
            ui::standby_set_ble_status(connected);
            if (connected) {
                ESP_LOGI(TAG, "Agent connected");
                hal::display_set_brightness(80);
            } else {
                ESP_LOGI(TAG, "Agent disconnected — standby");
                // Return to standby
                ui::standby_init();
                hal::display_set_brightness(20);
            }
            was_connected = connected;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
