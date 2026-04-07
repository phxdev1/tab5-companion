#pragma once
#include "lvgl.h"
#include <string>
#include <functional>
#include <memory>
#include <vector>

class App {
public:
    virtual ~App() = default;

    // Lifecycle — called by AppManager
    virtual void onCreate(lv_obj_t *parent) = 0;
    virtual void onShow() {}
    virtual void onHide() {}
    virtual void onDestroy() {}

    // Metadata (set by subclass constructor)
    const std::string &name() const { return name_; }
    const char *icon() const { return icon_; }
    uint32_t color() const { return color_; }

protected:
    std::string name_;
    const char *icon_ = LV_SYMBOL_DUMMY;
    uint32_t color_ = 0x333333;
};

struct AppEntry {
    std::string name;
    const char *icon;
    uint32_t color;
    std::function<std::unique_ptr<App>()> create;
};

class AppRegistry {
public:
    static AppRegistry &instance() {
        static AppRegistry reg;
        return reg;
    }

    void add(const char *name, const char *icon, uint32_t color,
             std::function<std::unique_ptr<App>()> factory) {
        entries_.push_back({name, icon, color, std::move(factory)});
    }

    const std::vector<AppEntry> &entries() const { return entries_; }

private:
    AppRegistry() = default;
    std::vector<AppEntry> entries_;
};

class AppManager {
public:
    static AppManager &instance() {
        static AppManager mgr;
        return mgr;
    }

    void init(lv_obj_t *root) { root_ = root; }

    void launch(size_t index) {
        auto &entries = AppRegistry::instance().entries();
        if (index >= entries.size()) return;

        close();

        active_ = entries[index].create();
        active_->onCreate(root_);
        active_->onShow();
    }

    void setLauncher(lv_obj_t *launcher) { launcher_ = launcher; }

    // Close active app, return to launcher
    void close() {
        if (!active_) return;

        // Unhide launcher FIRST so root screen is never exposed
        if (launcher_) lv_obj_remove_flag(launcher_, LV_OBJ_FLAG_HIDDEN);

        active_->onHide();
        active_->onDestroy();
        active_.reset();
    }

    bool hasActive() const { return active_ != nullptr; }

private:
    AppManager() = default;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *launcher_ = nullptr;
    std::unique_ptr<App> active_;
};
