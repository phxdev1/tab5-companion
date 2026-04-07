#pragma once
#include "lvgl.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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

// --- Touch ---
typedef void (*touch_handler_t)(int x, int y, int type); // type: 0=press, 1=release, 2=move

void touch_init();
void touch_enable_events(touch_handler_t handler);
void touch_disable_events();

// --- Audio ---
void audio_init();
void audio_tone(int freq_hz, int duration_ms);
void audio_set_volume(int pct);

}
