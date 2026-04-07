#include "hal/hal.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include <cstdio>

static const char *TAG = "hal:http";
static httpd_handle_t server_ = nullptr;

static esp_err_t health_handler(httpd_req_t *req)
{
    char buf[256];
    char ip[20] = {};
    hal::wifi_get_ip(ip, sizeof(ip));

    snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"device\":\"Tab5\",\"ip\":\"%s\",\"uptime\":%lld,\"heap\":%lu}",
        ip, (long long)(esp_timer_get_time() / 1000000), (unsigned long)esp_get_free_heap_size());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

void hal::http_start(uint16_t port)
{
    if (server_) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.stack_size = 8192;

    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    // Health endpoint
    httpd_uri_t health = {
        .uri = "/health",
        .method = HTTP_GET,
        .handler = health_handler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server_, &health);

    char ip[20] = {};
    hal::wifi_get_ip(ip, sizeof(ip));
    ESP_LOGI(TAG, "HTTP server on http://%s:%d", ip, port);
}

void hal::http_stop()
{
    if (server_) {
        httpd_stop(server_);
        server_ = nullptr;
    }
}
