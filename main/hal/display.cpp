#include "hal/hal.h"
#include "bsp/m5stack_tab5.h"
#include "esp_log.h"

static const char *TAG = "hal:display";

void hal::display_init()
{
    ESP_ERROR_CHECK(bsp_i2c_init());
    bsp_io_expander_init();

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_affinity = 1;
    port_cfg.task_max_sleep_ms = 50;
    port_cfg.task_priority = 3;

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = port_cfg,
        .buffer_size = BSP_LCD_H_RES * 100,
        .double_buffer = true,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = false,
        }
    };

    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    assert(disp != NULL);

    // Dark root screen before backlight comes on
    bsp_display_lock(0);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    bsp_display_unlock();

    ESP_LOGI(TAG, "Display initialized (%dx%d)", BSP_LCD_H_RES, BSP_LCD_V_RES);
}

void hal::display_set_brightness(int pct)
{
    bsp_display_brightness_set(pct);
}

void hal::display_lock()
{
    bsp_display_lock(0);
}

void hal::display_unlock()
{
    bsp_display_unlock();
}

lv_obj_t *hal::display_root()
{
    return lv_screen_active();
}
