#include "app.h"

// Shared back button handler — just close the active app
static void back_clicked(lv_event_t *e)
{
    AppManager::instance().close();
}

// Helper: add a standard back button to an app screen
static void add_back_button(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 80, 40);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 16, 12);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x222244), 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xaaaaaa), 0);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, back_clicked, LV_EVENT_CLICKED, nullptr);
}

// Helper: create a standard dark app screen with title and back button
static lv_obj_t *create_app_screen(lv_obj_t *parent, const char *title_text)
{
    lv_obj_t *screen = lv_obj_create(parent);
    lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0e0e1e), 0);

    add_back_button(screen);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    return screen;
}

// --- Settings ---
class SettingsApp : public App {
public:
    SettingsApp() { name_ = "Settings"; icon_ = LV_SYMBOL_SETTINGS; color_ = 0x6688cc; }
    void onCreate(lv_obj_t *parent) override {
        screen_ = create_app_screen(parent, LV_SYMBOL_SETTINGS "  Settings");
    }
    void onDestroy() override { if (screen_) { lv_obj_delete(screen_); screen_ = nullptr; } }
private:
    lv_obj_t *screen_ = nullptr;
};

// --- Wi-Fi ---
class WifiApp : public App {
public:
    WifiApp() { name_ = "Wi-Fi"; icon_ = LV_SYMBOL_WIFI; color_ = 0x44bb88; }
    void onCreate(lv_obj_t *parent) override {
        screen_ = create_app_screen(parent, LV_SYMBOL_WIFI "  Wi-Fi Scanner");
    }
    void onDestroy() override { if (screen_) { lv_obj_delete(screen_); screen_ = nullptr; } }
private:
    lv_obj_t *screen_ = nullptr;
};

// --- Registration ---
void register_apps()
{
    auto &reg = AppRegistry::instance();

    reg.add("Settings", LV_SYMBOL_SETTINGS, 0x6688cc,
            [] { return std::make_unique<SettingsApp>(); });

    reg.add("Wi-Fi", LV_SYMBOL_WIFI, 0x44bb88,
            [] { return std::make_unique<WifiApp>(); });

    reg.add("Bluetooth", LV_SYMBOL_BLUETOOTH, 0x5588ee,
            [] { return std::make_unique<SettingsApp>(); });

    reg.add("Files", LV_SYMBOL_SD_CARD, 0xcc8844,
            [] { return std::make_unique<SettingsApp>(); });

    reg.add("Audio", LV_SYMBOL_AUDIO, 0xcc4466,
            [] { return std::make_unique<SettingsApp>(); });

    reg.add("Terminal", LV_SYMBOL_LIST, 0x88cc44,
            [] { return std::make_unique<SettingsApp>(); });
}
