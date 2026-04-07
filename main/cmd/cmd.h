#pragma once
#include <stddef.h>

namespace cmd {
    void init();
    void execute(const char *json, size_t len);
}
