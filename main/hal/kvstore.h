#pragma once
#include <stddef.h>
#include <stdint.h>

namespace kv {

void init();

// Set a key with optional TTL (0 = no expiry)
void set(const char *key, const char *value, uint32_t ttl_s = 0);

// Get a key. Returns nullptr if not found or expired.
const char *get(const char *key);

// Delete a key
void del(const char *key);

// List all live keys as JSON array into buf. Returns count.
int list_json(char *buf, size_t len);

// Get a key as JSON response: {"ok":true,"key":"...","value":"..."}
int get_json(const char *key, char *buf, size_t len);

}

namespace events {

void init(size_t capacity = 256);

// Push an event (JSON string). Oldest dropped if full.
void push(const char *json);

// Read and drain up to limit events. Returns JSON array.
int read_json(char *buf, size_t len, int limit = 64);

// Clear all events
void clear();

// Count of pending events
int count();

}
