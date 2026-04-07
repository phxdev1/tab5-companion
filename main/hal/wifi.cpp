// Wi-Fi HAL — BLOCKED until C6 coprocessor is flashed with esp_hosted v2 slave firmware.
// The factory C6 runs esp_hosted v1 which is incompatible with the v2 host.
// Requires USB-TTL adapter to flash C6 download pads on PCB.

#include "hal/hal.h"
#include "esp_log.h"
#include <cstdio>

static const char *TAG = "hal:wifi";

void hal::wifi_init()
{
    ESP_LOGW(TAG, "Wi-Fi disabled — C6 needs esp_hosted v2 slave firmware (USB-TTL flash required)");
}

bool hal::wifi_connect(const char *ssid, const char *password)
{
    ESP_LOGE(TAG, "Wi-Fi blocked — C6 firmware upgrade required");
    return false;
}

bool hal::wifi_is_connected() { return false; }
void hal::wifi_get_ip(char *buf, size_t len) { snprintf(buf, len, "0.0.0.0"); }
int hal::wifi_get_rssi() { return 0; }
int hal::wifi_scan_json(char *buf, size_t len)
{
    snprintf(buf, len, "{\"error\":\"wifi blocked: C6 coprocessor needs esp_hosted v2 slave firmware. Flash via USB-TTL adapter.\"}");
    return -1;
}
