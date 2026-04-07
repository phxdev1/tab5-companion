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

// --- USB Host ---
void usb_init();
int usb_list_json(char *buf, size_t len);       // List connected devices
int usb_info_json(uint8_t addr, char *buf, size_t len);  // Device descriptors
int usb_control_transfer(uint8_t addr, uint8_t bmRequestType, uint8_t bRequest,
                         uint16_t wValue, uint16_t wIndex, uint8_t *data, uint16_t wLength,
                         char *resp_buf, size_t resp_len);

// --- Wi-Fi ---
void wifi_init();
bool wifi_connect(const char *ssid, const char *password);
bool wifi_is_connected();
void wifi_get_ip(char *buf, size_t len);
int wifi_get_rssi();
int wifi_scan_json(char *buf, size_t len); // Returns JSON array of APs

// --- HTTP Server ---
void http_start(uint16_t port = 8080);
void http_stop();

}

