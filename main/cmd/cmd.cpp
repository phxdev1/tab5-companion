#include "cmd/cmd.h"
#include "hal/hal.h"
#include "hal/kvstore.h"
#include "bsp/m5stack_tab5.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "cJSON.h"
#include <cstdio>
#include <cstring>

static const char *TAG = "cmd";

// --- Response helpers ---

static void respond_ok()
{
    hal::ble_notify("{\"ok\":true}");
}

static void respond_json(const char *json)
{
    hal::ble_notify(json);
}

static void respond_error(const char *msg)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", msg);
    hal::ble_notify(buf);
}

static uint32_t parse_color(const char *hex, uint32_t fallback)
{
    if (!hex || hex[0] != '#' || strlen(hex) < 7) return fallback;
    return (uint32_t)strtol(hex + 1, NULL, 16);
}

static const lv_font_t *pick_font(int size)
{
    if (size >= 48) return &lv_font_montserrat_48;
    if (size >= 24) return &lv_font_montserrat_24;
    return &lv_font_montserrat_14;
}

// ========== DISPLAY COMMANDS ==========

static void cmd_display_text(cJSON *root)
{
    const char *text = cJSON_GetStringValue(cJSON_GetObjectItem(root, "text"));
    if (!text) { respond_error("missing 'text'"); return; }

    const char *color_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "color"));
    uint32_t color = parse_color(color_str, 0xffffff);

    const char *bg_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "bg"));
    uint32_t bg = parse_color(bg_str, 0x000000);

    cJSON *size_item = cJSON_GetObjectItem(root, "size");
    int size = size_item ? size_item->valueint : 24;

    const char *pos = cJSON_GetStringValue(cJSON_GetObjectItem(root, "position"));

    cJSON *append_item = cJSON_GetObjectItem(root, "append");
    bool append = append_item && cJSON_IsTrue(append_item);

    hal::display_lock();
    lv_obj_t *scr = hal::display_root();

    if (!append) {
        lv_obj_clean(scr);
        lv_obj_set_style_bg_color(scr, lv_color_hex(bg), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    }

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, pick_font(size), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, lv_pct(90));

    if (pos && strcmp(pos, "top") == 0)
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);
    else if (pos && strcmp(pos, "bottom") == 0)
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -20);
    else
        lv_obj_center(label);

    hal::display_unlock();
    respond_ok();
}

static void cmd_display_clear(cJSON *root)
{
    const char *bg_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "bg"));
    uint32_t bg = parse_color(bg_str, 0x000000);

    hal::display_lock();
    lv_obj_t *scr = hal::display_root();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    hal::display_unlock();
    respond_ok();
}

static void cmd_display_brightness(cJSON *root)
{
    cJSON *val = cJSON_GetObjectItem(root, "value");
    if (!val) { respond_error("missing 'value'"); return; }
    hal::display_set_brightness(val->valueint);
    respond_ok();
}

static void cmd_display_rect(cJSON *root)
{
    cJSON *w_item = cJSON_GetObjectItem(root, "w");
    cJSON *h_item = cJSON_GetObjectItem(root, "h");
    if (!w_item || !h_item) { respond_error("missing 'w' or 'h'"); return; }

    int x = cJSON_GetObjectItem(root, "x") ? cJSON_GetObjectItem(root, "x")->valueint : 0;
    int y = cJSON_GetObjectItem(root, "y") ? cJSON_GetObjectItem(root, "y")->valueint : 0;
    int w = w_item->valueint;
    int h = h_item->valueint;
    const char *color_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "color"));
    uint32_t color = parse_color(color_str, 0xffffff);
    cJSON *radius_item = cJSON_GetObjectItem(root, "radius");
    int radius = radius_item ? radius_item->valueint : 0;

    hal::display_lock();
    lv_obj_t *rect = lv_obj_create(hal::display_root());
    lv_obj_remove_style_all(rect);
    lv_obj_set_pos(rect, x, y);
    lv_obj_set_size(rect, w, h);
    lv_obj_set_style_bg_color(rect, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rect, radius, 0);
    hal::display_unlock();
    respond_ok();
}

static void cmd_display_color(cJSON *root)
{
    const char *color_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "color"));
    if (!color_str) { respond_error("missing 'color'"); return; }
    uint32_t color = parse_color(color_str, 0x000000);

    hal::display_lock();
    lv_obj_t *scr = hal::display_root();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    hal::display_unlock();
    respond_ok();
}

static void cmd_display_screen_size(cJSON *root)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"ok\":true,\"width\":%d,\"height\":%d}",
             BSP_LCD_H_RES, BSP_LCD_V_RES);
    respond_json(buf);
}

static void cmd_display_progress(cJSON *root)
{
    cJSON *val = cJSON_GetObjectItem(root, "value");
    if (!val) { respond_error("missing 'value'"); return; }
    int pct = val->valueint;

    const char *color_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "color"));
    uint32_t color = parse_color(color_str, 0x4488ff);

    const char *label_text = cJSON_GetStringValue(cJSON_GetObjectItem(root, "label"));

    hal::display_lock();
    lv_obj_t *scr = hal::display_root();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Progress bar
    lv_obj_t *bar = lv_bar_create(scr);
    lv_obj_set_size(bar, lv_pct(70), 16);
    lv_obj_center(bar);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 8, 0);
    lv_obj_set_style_radius(bar, 8, LV_PART_INDICATOR);

    if (label_text) {
        lv_obj_t *lbl = lv_label_create(scr);
        lv_label_set_text(lbl, label_text);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xcccccc), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_align_to(lbl, bar, LV_ALIGN_OUT_TOP_MID, 0, -16);
    }

    hal::display_unlock();
    respond_ok();
}

// ========== TOUCH COMMANDS ==========

static void on_touch_event(int x, int y, int type)
{
    const char *types[] = {"press", "release", "move"};

    // Only queue press/release events — moves flood the buffer
    if (type == 0 || type == 1) {
        char buf[128];
        snprintf(buf, sizeof(buf),
            "{\"type\":\"touch\",\"x\":%d,\"y\":%d,\"action\":\"%s\",\"t\":%lld}",
            x, y, types[type], (long long)(esp_timer_get_time() / 1000));
        events::push(buf);
    }

    // KV always gets latest position (including moves)
    char kv_buf[64];
    snprintf(kv_buf, sizeof(kv_buf), "{\"x\":%d,\"y\":%d,\"action\":\"%s\"}", x, y, types[type]);
    kv::set("touch.last", kv_buf, 30);
}

static void cmd_touch_enable(cJSON *root)
{
    hal::touch_enable_events(on_touch_event);
    respond_ok();
}

static void cmd_touch_disable(cJSON *root)
{
    hal::touch_disable_events();
    respond_ok();
}

// ========== AUDIO COMMANDS ==========

static void cmd_audio_tone(cJSON *root)
{
    cJSON *freq_item = cJSON_GetObjectItem(root, "freq");
    cJSON *duration_item = cJSON_GetObjectItem(root, "duration_ms");

    int freq = freq_item ? freq_item->valueint : 1000;
    int duration = duration_item ? duration_item->valueint : 200;

    hal::audio_tone(freq, duration);
    respond_ok();
}

static void cmd_audio_volume(cJSON *root)
{
    cJSON *val = cJSON_GetObjectItem(root, "value");
    if (!val) { respond_error("missing 'value'"); return; }
    hal::audio_set_volume(val->valueint);
    respond_ok();
}

// ========== USB HOST COMMANDS ==========

static void cmd_usb_list(cJSON *root)
{
    char *buf = (char *)malloc(2048);
    if (!buf) { respond_error("no memory"); return; }
    hal::usb_list_json(buf, 2048);
    respond_json(buf);
    free(buf);
}

static void cmd_usb_info(cJSON *root)
{
    cJSON *addr_item = cJSON_GetObjectItem(root, "addr");
    if (!addr_item) { respond_error("missing 'addr'"); return; }
    char *buf = (char *)malloc(2048);
    if (!buf) { respond_error("no memory"); return; }
    hal::usb_info_json((uint8_t)addr_item->valueint, buf, 2048);
    respond_json(buf);
    free(buf);
}

static void cmd_usb_control(cJSON *root)
{
    cJSON *addr_item = cJSON_GetObjectItem(root, "addr");
    cJSON *rt_item = cJSON_GetObjectItem(root, "bmRequestType");
    cJSON *req_item = cJSON_GetObjectItem(root, "bRequest");
    if (!addr_item || !rt_item || !req_item) {
        respond_error("missing addr, bmRequestType, or bRequest");
        return;
    }

    uint8_t addr = (uint8_t)addr_item->valueint;
    uint8_t bmRT = (uint8_t)rt_item->valueint;
    uint8_t bReq = (uint8_t)req_item->valueint;
    uint16_t wVal = cJSON_GetObjectItem(root, "wValue") ? (uint16_t)cJSON_GetObjectItem(root, "wValue")->valueint : 0;
    uint16_t wIdx = cJSON_GetObjectItem(root, "wIndex") ? (uint16_t)cJSON_GetObjectItem(root, "wIndex")->valueint : 0;
    uint16_t wLen = cJSON_GetObjectItem(root, "wLength") ? (uint16_t)cJSON_GetObjectItem(root, "wLength")->valueint : 0;

    // For OUT transfers, parse data array
    uint8_t *data = NULL;
    cJSON *data_arr = cJSON_GetObjectItem(root, "data");
    if (data_arr && cJSON_IsArray(data_arr) && cJSON_GetArraySize(data_arr) > 0) {
        int n = cJSON_GetArraySize(data_arr);
        data = (uint8_t *)malloc(n);
        for (int i = 0; i < n; i++) {
            data[i] = (uint8_t)cJSON_GetArrayItem(data_arr, i)->valueint;
        }
        if (wLen == 0) wLen = (uint16_t)n;
    }

    char *buf = (char *)malloc(2048);
    if (!buf) { free(data); respond_error("no memory"); return; }

    hal::usb_control_transfer(addr, bmRT, bReq, wVal, wIdx, data, wLen, buf, 2048);
    respond_json(buf);

    free(buf);
    free(data);
}

// ========== WIFI COMMANDS ==========

static void wifi_scan_task(void *arg)
{
    char *buf = (char *)malloc(4096);
    if (buf) {
        hal::wifi_scan_json(buf, 4096);
        hal::ble_notify(buf);
        free(buf);
    }
    vTaskDelete(NULL);
}

static void cmd_wifi_scan(cJSON *root)
{
    // Non-blocking — scan runs on separate task, result via BLE notify
    xTaskCreate(wifi_scan_task, "wifi_scan", 4096, NULL, 3, NULL);
    respond_json("{\"ok\":true,\"note\":\"scanning async, watch for result\"}");
}

static void cmd_wifi_connect(cJSON *root)
{
    const char *ssid = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ssid"));
    if (!ssid) { respond_error("missing 'ssid'"); return; }

    const char *password = cJSON_GetStringValue(cJSON_GetObjectItem(root, "password"));
    if (!password) password = "";

    // Non-blocking — kicks off a task, result arrives as BLE notification
    // {event:"wifi", status:"connected"|"failed", ssid:..., ip:...}
    bool started = hal::wifi_connect(ssid, password);
    if (started) {
        respond_json("{\"ok\":true,\"note\":\"connecting async, watch for wifi event\"}");
    } else {
        respond_error("wifi not initialized");
    }
}

static void cmd_wifi_status(cJSON *root)
{
    char ip[20] = {};
    hal::wifi_get_ip(ip, sizeof(ip));
    int rssi = hal::wifi_get_rssi();

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"connected\":%s,\"ip\":\"%s\",\"rssi\":%d}",
        hal::wifi_is_connected() ? "true" : "false", ip, rssi);
    respond_json(buf);
}

static void cmd_wifi_disconnect(cJSON *root)
{
    hal::http_stop();
    // esp_wifi_disconnect handled internally
    respond_ok();
}

// ========== KV COMMANDS ==========

static void cmd_kv_get(cJSON *root)
{
    const char *key = cJSON_GetStringValue(cJSON_GetObjectItem(root, "key"));
    if (!key) { respond_error("missing 'key'"); return; }
    char *buf = (char *)malloc(2048);
    if (!buf) { respond_error("no memory"); return; }
    kv::get_json(key, buf, 2048);
    respond_json(buf);
    free(buf);
}

static void cmd_kv_set(cJSON *root)
{
    const char *key = cJSON_GetStringValue(cJSON_GetObjectItem(root, "key"));
    if (!key) { respond_error("missing 'key'"); return; }
    cJSON *val = cJSON_GetObjectItem(root, "value");
    if (!val) { respond_error("missing 'value'"); return; }
    char *val_str = cJSON_PrintUnformatted(val);
    ESP_LOGI("kv", "set key=%s type=%d raw=%s", key, val->type, val_str);
    cJSON *ttl_item = cJSON_GetObjectItem(root, "ttl");
    uint32_t ttl = ttl_item ? (uint32_t)ttl_item->valueint : 0;
    kv::set(key, val_str, ttl);
    free(val_str);
    respond_ok();
}

static void cmd_kv_delete(cJSON *root)
{
    const char *key = cJSON_GetStringValue(cJSON_GetObjectItem(root, "key"));
    if (!key) { respond_error("missing 'key'"); return; }
    kv::del(key);
    respond_ok();
}

static void cmd_kv_list(cJSON *root)
{
    char *buf = (char *)malloc(4096);
    if (!buf) { respond_error("no memory"); return; }
    kv::list_json(buf, 4096);
    respond_json(buf);
    free(buf);
}

// ========== EVENT COMMANDS ==========

static void cmd_events_read(cJSON *root)
{
    cJSON *limit_item = cJSON_GetObjectItem(root, "limit");
    int limit = limit_item ? limit_item->valueint : 3; // Default 3 — fits in any BLE MTU
    if (limit < 1) limit = 1;
    if (limit > 20) limit = 20;

    size_t buf_size = (size_t)limit * 120 + 80;
    char *buf = (char *)malloc(buf_size);
    if (!buf) { respond_error("no memory"); return; }
    events::read_json(buf, buf_size, limit);
    respond_json(buf);
    free(buf);
}

static void cmd_events_clear(cJSON *root)
{
    events::clear();
    respond_ok();
}

static void cmd_events_ack(cJSON *root)
{
    cJSON *count_item = cJSON_GetObjectItem(root, "count");
    if (!count_item) { respond_error("missing 'count'"); return; }
    events::ack(count_item->valueint);
    respond_ok();
}

static void cmd_events_count(cJSON *root)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"ok\":true,\"count\":%d}", events::count());
    respond_json(buf);
}

// ========== SYSTEM COMMANDS ==========

static void cmd_system_info(cJSON *root)
{
    int64_t uptime_s = esp_timer_get_time() / 1000000;
    size_t heap_free = esp_get_free_heap_size();
    size_t heap_min = esp_get_minimum_free_heap_size();

    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"ok\":true,"
        "\"heap_free\":%zu,"
        "\"heap_min\":%zu,"
        "\"uptime_s\":%lld,"
        "\"ble_connected\":%s,"
        "\"display\":{\"width\":%d,\"height\":%d}"
        "}",
        heap_free, heap_min, (long long)uptime_s,
        hal::ble_is_connected() ? "true" : "false",
        BSP_LCD_H_RES, BSP_LCD_V_RES);
    respond_json(buf);
}

static void cmd_system_reboot(cJSON *root)
{
    respond_ok();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

static void cmd_system_sleep(cJSON *root)
{
    cJSON *secs = cJSON_GetObjectItem(root, "seconds");
    int sleep_s = secs ? secs->valueint : 0;

    respond_ok();
    vTaskDelay(pdMS_TO_TICKS(200));
    hal::display_set_brightness(0);

    if (sleep_s > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)sleep_s * 1000000ULL);
    }
    esp_deep_sleep_start();
}

static void cmd_system_ping(cJSON *root)
{
    respond_json("{\"ok\":true,\"pong\":true}");
}

// ========== DISPATCH ==========

static void on_command(const char *json, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root) { respond_error("invalid JSON"); return; }

    const char *cmd = cJSON_GetStringValue(cJSON_GetObjectItem(root, "cmd"));
    if (!cmd) { respond_error("missing 'cmd'"); cJSON_Delete(root); return; }

    // Display
    if      (strcmp(cmd, "display.text") == 0)        cmd_display_text(root);
    else if (strcmp(cmd, "display.clear") == 0)       cmd_display_clear(root);
    else if (strcmp(cmd, "display.brightness") == 0)  cmd_display_brightness(root);
    else if (strcmp(cmd, "display.rect") == 0)        cmd_display_rect(root);
    else if (strcmp(cmd, "display.color") == 0)       cmd_display_color(root);
    else if (strcmp(cmd, "display.screen_size") == 0) cmd_display_screen_size(root);
    else if (strcmp(cmd, "display.progress") == 0)    cmd_display_progress(root);
    // Touch
    else if (strcmp(cmd, "touch.enable") == 0)        cmd_touch_enable(root);
    else if (strcmp(cmd, "touch.disable") == 0)       cmd_touch_disable(root);
    // Audio
    else if (strcmp(cmd, "audio.tone") == 0)           cmd_audio_tone(root);
    else if (strcmp(cmd, "audio.volume") == 0)         cmd_audio_volume(root);
    // USB Host
    else if (strcmp(cmd, "usb.list") == 0)              cmd_usb_list(root);
    else if (strcmp(cmd, "usb.info") == 0)              cmd_usb_info(root);
    else if (strcmp(cmd, "usb.control") == 0)           cmd_usb_control(root);
    // Wi-Fi
    else if (strcmp(cmd, "wifi.scan") == 0)             cmd_wifi_scan(root);
    else if (strcmp(cmd, "wifi.connect") == 0)          cmd_wifi_connect(root);
    else if (strcmp(cmd, "wifi.status") == 0)           cmd_wifi_status(root);
    else if (strcmp(cmd, "wifi.disconnect") == 0)       cmd_wifi_disconnect(root);
    // KV Store
    else if (strcmp(cmd, "kv.get") == 0)                cmd_kv_get(root);
    else if (strcmp(cmd, "kv.set") == 0)                cmd_kv_set(root);
    else if (strcmp(cmd, "kv.delete") == 0)             cmd_kv_delete(root);
    else if (strcmp(cmd, "kv.list") == 0)               cmd_kv_list(root);
    // Events
    else if (strcmp(cmd, "events.read") == 0)           cmd_events_read(root);
    else if (strcmp(cmd, "events.ack") == 0)            cmd_events_ack(root);
    else if (strcmp(cmd, "events.clear") == 0)          cmd_events_clear(root);
    else if (strcmp(cmd, "events.count") == 0)          cmd_events_count(root);
    // System
    else if (strcmp(cmd, "system.info") == 0)          cmd_system_info(root);
    else if (strcmp(cmd, "system.reboot") == 0)        cmd_system_reboot(root);
    else if (strcmp(cmd, "system.sleep") == 0)         cmd_system_sleep(root);
    else if (strcmp(cmd, "system.ping") == 0)          cmd_system_ping(root);
    else {
        ESP_LOGW(TAG, "Unknown command: %s", cmd);
        respond_error("unknown command");
    }

    cJSON_Delete(root);
}

void cmd::init()
{
    hal::ble_set_command_handler(on_command);
    ESP_LOGI(TAG, "Command dispatch ready");
}

void cmd::execute(const char *json, size_t len)
{
    on_command(json, len);
}
