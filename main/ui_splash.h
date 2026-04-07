#pragma once
#include "lvgl.h"
#include <functional>

void ui_splash_create(lv_obj_t *parent, std::function<void()> on_done);
