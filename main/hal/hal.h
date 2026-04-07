#pragma once
#include "lvgl.h"
#include <stddef.h>

namespace hal {

// --- Display ---
void display_init();
void display_set_brightness(int pct);
void display_lock();
void display_unlock();
lv_obj_t *display_root();

// --- BLE ---
typedef void (*command_handler_t)(const char *json, size_t len);

void ble_init();
bool ble_is_connected();
void ble_notify(const char *json);
void ble_set_command_handler(command_handler_t handler);

}
