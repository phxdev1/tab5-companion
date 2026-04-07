#include "ui_launcher.h"
#include "app.h"

static lv_obj_t *launcher_scr = nullptr;
static lv_obj_t *grid = nullptr;

static void card_click_cb(lv_event_t *e)
{
    size_t idx = reinterpret_cast<size_t>(lv_event_get_user_data(e));
    auto &mgr = AppManager::instance();

    // Hide launcher, launch app
    if (launcher_scr) lv_obj_add_flag(launcher_scr, LV_OBJ_FLAG_HIDDEN);
    mgr.launch(idx);
}

static lv_obj_t *create_card(lv_obj_t *parent, const AppEntry &entry, size_t idx)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 180, 160);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_pad_row(card, 12, 0);

    // Card background
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_radius(card, 20, 0);

    // Subtle border
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);

    // Shadow
    lv_obj_set_style_shadow_width(card, 30, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_40, 0);
    lv_obj_set_style_shadow_offset_y(card, 8, 0);

    // Pressed state
    lv_obj_set_style_bg_color(card, lv_color_hex(entry.color), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(card, LV_OPA_30, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_x(card, 240, LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale_y(card, 240, LV_STATE_PRESSED);

    // Transition
    static const lv_style_prop_t props[] = {
        LV_STYLE_TRANSFORM_SCALE_X, LV_STYLE_TRANSFORM_SCALE_Y,
        LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA, LV_STYLE_PROP_INV
    };
    static lv_style_transition_dsc_t tr;
    lv_style_transition_dsc_init(&tr, props, lv_anim_path_ease_out, 150, 0, nullptr);
    lv_obj_set_style_transition(card, &tr, LV_STATE_PRESSED);
    lv_obj_set_style_transition(card, &tr, LV_STATE_DEFAULT);

    // Icon circle
    lv_obj_t *icon_bg = lv_obj_create(card);
    lv_obj_remove_style_all(icon_bg);
    lv_obj_set_size(icon_bg, 56, 56);
    lv_obj_set_style_bg_opa(icon_bg, LV_OPA_20, 0);
    lv_obj_set_style_bg_color(icon_bg, lv_color_hex(entry.color), 0);
    lv_obj_set_style_radius(icon_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(icon_bg, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t *icon = lv_label_create(icon_bg);
    lv_label_set_text(icon, entry.icon);
    lv_obj_set_style_text_color(icon, lv_color_hex(entry.color), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_center(icon);

    // Label
    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, entry.name.c_str());
    lv_obj_set_style_text_color(label, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Clickable
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, card_click_cb, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(idx));

    return card;
}

void ui_launcher_create(lv_obj_t *parent)
{
    launcher_scr = lv_obj_create(parent);
    lv_obj_remove_style_all(launcher_scr);
    lv_obj_set_size(launcher_scr, LV_PCT(100), LV_PCT(100));

    // Tell AppManager about us so close() can unhide us
    AppManager::instance().setLauncher(launcher_scr);

    // Background
    lv_obj_set_style_bg_opa(launcher_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(launcher_scr, lv_color_hex(0x0e0e1e), 0);
    lv_obj_set_style_bg_grad_color(launcher_scr, lv_color_hex(0x14142a), 0);
    lv_obj_set_style_bg_grad_dir(launcher_scr, LV_GRAD_DIR_VER, 0);

    // Status bar
    lv_obj_t *bar = lv_obj_create(launcher_scr);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), 40);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_pad_hor(bar, 24, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *clock_lbl = lv_label_create(bar);
    lv_label_set_text(clock_lbl, "12:00");
    lv_obj_set_style_text_color(clock_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(clock_lbl, &lv_font_montserrat_14, 0);

    lv_obj_t *status = lv_label_create(bar);
    lv_label_set_text(status, LV_SYMBOL_WIFI "  " LV_SYMBOL_BLUETOOTH "  " LV_SYMBOL_BATTERY_3);
    lv_obj_set_style_text_color(status, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);

    // App grid
    grid = lv_obj_create(launcher_scr);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_top(grid, 56, 0);
    lv_obj_set_style_pad_hor(grid, 40, 0);
    lv_obj_set_style_pad_row(grid, 20, 0);
    lv_obj_set_style_pad_column(grid, 20, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(grid, LV_SCROLL_SNAP_START);

    // Populate from registry
    auto &entries = AppRegistry::instance().entries();
    for (size_t i = 0; i < entries.size(); i++) {
        lv_obj_t *card = create_card(grid, entries[i], i);
        // Staggered fade-in
        lv_obj_set_style_opa(card, LV_OPA_TRANSP, 0);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, card);
        lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_duration(&a, 400);
        lv_anim_set_delay(&a, i * 80);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
            lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), v, 0);
        });
        lv_anim_start(&a);
    }
}

void ui_launcher_destroy(void)
{
    if (launcher_scr) {
        lv_obj_delete(launcher_scr);
        launcher_scr = nullptr;
        grid = nullptr;
    }
}
