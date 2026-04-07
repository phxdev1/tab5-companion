#include "ui/ui.h"
#include "hal/hal.h"
#include "esp_log.h"

static lv_obj_t *ble_icon = nullptr;
static lv_anim_t pulse_anim;

static void pulse_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), v, 0);
}

void ui::standby_init()
{
    hal::display_lock();
    lv_obj_t *scr = hal::display_root();
    lv_obj_clean(scr);

    // Small BLE icon in bottom-right corner
    ble_icon = lv_label_create(scr);
    lv_label_set_text(ble_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(ble_icon, lv_color_hex(0x3355aa), 0);
    lv_obj_set_style_text_font(ble_icon, &lv_font_montserrat_14, 0);
    lv_obj_align(ble_icon, LV_ALIGN_BOTTOM_RIGHT, -16, -12);

    // Pulse animation (advertising indicator)
    lv_anim_init(&pulse_anim);
    lv_anim_set_var(&pulse_anim, ble_icon);
    lv_anim_set_values(&pulse_anim, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_duration(&pulse_anim, 1200);
    lv_anim_set_playback_duration(&pulse_anim, 1200);
    lv_anim_set_repeat_count(&pulse_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&pulse_anim, pulse_cb);
    lv_anim_start(&pulse_anim);

    hal::display_unlock();
}

void ui::standby_set_ble_status(bool connected)
{
    if (!ble_icon) return;

    hal::display_lock();
    if (connected) {
        // Stop pulsing, solid dim icon
        lv_anim_delete(ble_icon, pulse_cb);
        lv_obj_set_style_opa(ble_icon, LV_OPA_40, 0);
        lv_obj_set_style_text_color(ble_icon, lv_color_hex(0x226644), 0);
    } else {
        // Resume pulsing blue
        lv_obj_set_style_text_color(ble_icon, lv_color_hex(0x3355aa), 0);
        lv_anim_start(&pulse_anim);
    }
    hal::display_unlock();
}
