#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "bsp/m5stack_tab5.h"
#include "lvgl.h"
#include "esp_log.h"
#include "app.h"
#include "ui_splash.h"
#include "ui_launcher.h"

static const char *TAG = "tab5";

extern void register_apps();

static void show_launcher()
{
    // Called from LVGL animation context — lock already held
    lv_obj_t *scr = lv_screen_active();
    AppManager::instance().init(scr);
    ui_launcher_create(scr);
    ESP_LOGI(TAG, "Launcher ready");
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(bsp_i2c_init());
    bsp_io_expander_init();

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_affinity = 1;  // Run LVGL on CPU 1 so CPU 0 IDLE isn't starved

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = port_cfg,
        .buffer_size = BSP_LCD_H_RES * 100,
        .double_buffer = true,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        }
    };
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    assert(disp != NULL);
    // Backlight stays off (BSP inits LEDC with duty=0)

    register_apps();

    // Set root screen to dark + create splash while backlight is still off
    bsp_display_lock(0);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    ui_splash_create(scr, show_launcher);

    bsp_display_unlock();

    // Give LVGL time to flush the dark frame before turning on backlight
    vTaskDelay(pdMS_TO_TICKS(200));
    bsp_display_brightness_set(80);

    ESP_LOGI(TAG, "Boot complete");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
