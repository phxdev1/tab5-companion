#include "hal/kvstore.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

static const char *TAG = "kv";

// ========== KV STORE ==========

struct kv_entry_t {
    char key[64];
    char *value;        // heap-allocated
    int64_t expires_us; // 0 = no expiry
    kv_entry_t *next;
};

static kv_entry_t *kv_head_ = nullptr;
static SemaphoreHandle_t kv_mutex_ = nullptr;

static int64_t now_us() { return esp_timer_get_time(); }

static kv_entry_t *find_entry(const char *key)
{
    for (kv_entry_t *e = kv_head_; e; e = e->next) {
        if (strcmp(e->key, key) == 0) return e;
    }
    return nullptr;
}

static void gc_expired()
{
    int64_t now = now_us();
    kv_entry_t **pp = &kv_head_;
    while (*pp) {
        kv_entry_t *e = *pp;
        if (e->expires_us > 0 && now > e->expires_us) {
            *pp = e->next;
            free(e->value);
            free(e);
        } else {
            pp = &e->next;
        }
    }
}

static void gc_task(void *arg)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        xSemaphoreTake(kv_mutex_, portMAX_DELAY);
        gc_expired();
        xSemaphoreGive(kv_mutex_);
    }
}

void kv::init()
{
    kv_mutex_ = xSemaphoreCreateMutex();
    xTaskCreate(gc_task, "kv_gc", 2048, NULL, 1, NULL);
    ESP_LOGI(TAG, "KV store initialized (GC every 5s)");
}

void kv::set(const char *key, const char *value, uint32_t ttl_s)
{
    xSemaphoreTake(kv_mutex_, portMAX_DELAY);

    kv_entry_t *e = find_entry(key);
    if (!e) {
        e = (kv_entry_t *)calloc(1, sizeof(kv_entry_t));
        strncpy(e->key, key, sizeof(e->key) - 1);
        e->next = kv_head_;
        kv_head_ = e;
    } else {
        free(e->value);
    }

    e->value = strdup(value);
    e->expires_us = ttl_s > 0 ? now_us() + (int64_t)ttl_s * 1000000 : 0;

    xSemaphoreGive(kv_mutex_);
}

const char *kv::get(const char *key)
{
    xSemaphoreTake(kv_mutex_, portMAX_DELAY);
    kv_entry_t *e = find_entry(key);
    const char *val = nullptr;
    if (e) {
        if (e->expires_us > 0 && now_us() > e->expires_us) {
            // Expired — don't return it
        } else {
            val = e->value;
        }
    }
    xSemaphoreGive(kv_mutex_);
    return val;
}

void kv::del(const char *key)
{
    xSemaphoreTake(kv_mutex_, portMAX_DELAY);
    kv_entry_t **pp = &kv_head_;
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            kv_entry_t *e = *pp;
            *pp = e->next;
            free(e->value);
            free(e);
            break;
        }
        pp = &(*pp)->next;
    }
    xSemaphoreGive(kv_mutex_);
}

int kv::list_json(char *buf, size_t len)
{
    xSemaphoreTake(kv_mutex_, portMAX_DELAY);
    int64_t now = now_us();
    int pos = snprintf(buf, len, "{\"ok\":true,\"keys\":[");
    int count = 0;

    for (kv_entry_t *e = kv_head_; e && pos < (int)len - 80; e = e->next) {
        if (e->expires_us > 0 && now > e->expires_us) continue;
        if (count > 0) pos += snprintf(buf + pos, len - pos, ",");
        pos += snprintf(buf + pos, len - pos, "\"%s\"", e->key);
        count++;
    }

    snprintf(buf + pos, len - pos, "],\"count\":%d}", count);
    xSemaphoreGive(kv_mutex_);
    return count;
}

int kv::get_json(const char *key, char *buf, size_t len)
{
    const char *val = kv::get(key);
    if (val) {
        snprintf(buf, len, "{\"ok\":true,\"key\":\"%s\",\"value\":%s}", key, val);
        return 0;
    }
    snprintf(buf, len, "{\"error\":\"key not found\",\"key\":\"%s\"}", key);
    return -1;
}

// ========== EVENT QUEUE ==========

struct event_entry_t {
    char *json;
};

static event_entry_t *event_buf_ = nullptr;
static size_t event_cap_ = 0;
static size_t event_head_ = 0;  // next write position
static size_t event_count_ = 0;
static SemaphoreHandle_t event_mutex_ = nullptr;

void events::init(size_t capacity)
{
    event_mutex_ = xSemaphoreCreateMutex();
    event_cap_ = capacity;
    event_buf_ = (event_entry_t *)calloc(capacity, sizeof(event_entry_t));
    ESP_LOGI(TAG, "Event queue initialized (capacity=%zu)", capacity);
}

void events::push(const char *json)
{
    xSemaphoreTake(event_mutex_, portMAX_DELAY);

    // If slot occupied (ring wrapped), free old
    if (event_buf_[event_head_].json) {
        free(event_buf_[event_head_].json);
    }

    event_buf_[event_head_].json = strdup(json);
    event_head_ = (event_head_ + 1) % event_cap_;
    if (event_count_ < event_cap_) event_count_++;

    xSemaphoreGive(event_mutex_);
}

int events::read_json(char *buf, size_t len, int limit)
{
    xSemaphoreTake(event_mutex_, portMAX_DELAY);

    int to_read = event_count_ < (size_t)limit ? (int)event_count_ : limit;
    int remaining = (int)event_count_ - to_read;

    // Calculate start position (oldest unread)
    size_t start;
    if (event_count_ >= event_cap_) {
        start = event_head_;
    } else {
        start = (event_head_ + event_cap_ - event_count_) % event_cap_;
    }

    int pos = snprintf(buf, len, "{\"ok\":true,\"count\":%d,\"remaining\":%d,\"events\":[",
                       to_read, remaining);

    for (int i = 0; i < to_read && pos < (int)len - 20; i++) {
        size_t idx = (start + i) % event_cap_;
        if (!event_buf_[idx].json) continue;
        if (i > 0) pos += snprintf(buf + pos, len - pos, ",");
        pos += snprintf(buf + pos, len - pos, "%s", event_buf_[idx].json);
    }

    snprintf(buf + pos, len - pos, "]}");

    // Drain only what we returned — leave the rest
    for (int i = 0; i < to_read; i++) {
        size_t idx = (start + i) % event_cap_;
        if (event_buf_[idx].json) {
            free(event_buf_[idx].json);
            event_buf_[idx].json = nullptr;
        }
    }
    event_count_ -= to_read;

    xSemaphoreGive(event_mutex_);
    return to_read;
}

void events::clear()
{
    xSemaphoreTake(event_mutex_, portMAX_DELAY);
    for (size_t i = 0; i < event_cap_; i++) {
        if (event_buf_[i].json) {
            free(event_buf_[i].json);
            event_buf_[i].json = nullptr;
        }
    }
    event_count_ = 0;
    event_head_ = 0;
    xSemaphoreGive(event_mutex_);
}

int events::count()
{
    xSemaphoreTake(event_mutex_, portMAX_DELAY);
    int c = (int)event_count_;
    xSemaphoreGive(event_mutex_);
    return c;
}
