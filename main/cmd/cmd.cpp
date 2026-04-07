#include "cmd/cmd.h"
#include "hal/hal.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <cstdio>
#include <cstring>

static const char *TAG = "cmd";

static void respond_ok()
{
    hal::ble_notify("{\"ok\":true}");
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

// --- Command handlers ---

static void cmd_display_text(cJSON *root)
{
    const char *text = cJSON_GetStringValue(cJSON_GetObjectItem(root, "text"));
    if (!text) { respond_error("missing 'text'"); return; }

    const char *color_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "color"));
    uint32_t color = parse_color(color_str, 0xffffff);

    cJSON *size_item = cJSON_GetObjectItem(root, "size");
    int size = size_item ? size_item->valueint : 24;

    const lv_font_t *font = &lv_font_montserrat_24;
    if (size >= 48) font = &lv_font_montserrat_48;
    else if (size <= 14) font = &lv_font_montserrat_14;

    hal::display_lock();
    lv_obj_t *scr = hal::display_root();

    // Clear previous content
    lv_obj_clean(scr);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, lv_pct(90));
    lv_obj_center(label);

    hal::display_unlock();

    ESP_LOGI(TAG, "display.text: \"%s\"", text);
    respond_ok();
}

static void cmd_display_clear(cJSON *root)
{
    hal::display_lock();
    lv_obj_clean(hal::display_root());
    hal::display_unlock();

    ESP_LOGI(TAG, "display.clear");
    respond_ok();
}

static void cmd_display_brightness(cJSON *root)
{
    cJSON *val = cJSON_GetObjectItem(root, "value");
    if (!val) { respond_error("missing 'value'"); return; }

    int pct = val->valueint;
    hal::display_set_brightness(pct);

    ESP_LOGI(TAG, "display.brightness: %d%%", pct);
    respond_ok();
}

static void cmd_system_info(cJSON *root)
{
    int64_t uptime_s = esp_timer_get_time() / 1000000;
    size_t heap = esp_get_free_heap_size();

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"heap\":%zu,\"uptime\":%lld,\"ble\":true}",
        heap, (long long)uptime_s);

    hal::ble_notify(buf);
}

static void cmd_system_reboot(cJSON *root)
{
    respond_ok();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

// --- Dispatch ---

static void on_command(const char *json, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root) {
        respond_error("invalid JSON");
        return;
    }

    const char *cmd = cJSON_GetStringValue(cJSON_GetObjectItem(root, "cmd"));
    if (!cmd) {
        respond_error("missing 'cmd'");
        cJSON_Delete(root);
        return;
    }

    if (strcmp(cmd, "display.text") == 0)           cmd_display_text(root);
    else if (strcmp(cmd, "display.clear") == 0)      cmd_display_clear(root);
    else if (strcmp(cmd, "display.brightness") == 0)  cmd_display_brightness(root);
    else if (strcmp(cmd, "system.info") == 0)         cmd_system_info(root);
    else if (strcmp(cmd, "system.reboot") == 0)       cmd_system_reboot(root);
    else {
        ESP_LOGW(TAG, "Unknown command: %s", cmd);
        respond_error("unknown command");
    }

    cJSON_Delete(root);
}

// --- Public API ---

void cmd::init()
{
    hal::ble_set_command_handler(on_command);
    ESP_LOGI(TAG, "Command dispatch ready");
}

void cmd::execute(const char *json, size_t len)
{
    on_command(json, len);
}
