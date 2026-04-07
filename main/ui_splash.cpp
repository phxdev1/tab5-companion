#include "ui_splash.h"

static std::function<void()> s_on_done;

static void fade_out_cb(lv_anim_t *a)
{
    lv_obj_t *splash = static_cast<lv_obj_t *>(lv_anim_get_user_data(a));
    lv_obj_delete(splash);
    if (s_on_done) s_on_done();
}

static void trigger_exit(lv_timer_t *timer)
{
    lv_obj_t *splash = static_cast<lv_obj_t *>(lv_timer_get_user_data(timer));
    lv_timer_delete(timer);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, splash);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, 600);
    lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
        lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), v, 0);
    });
    lv_anim_set_completed_cb(&a, fade_out_cb);
    lv_anim_set_user_data(&a, splash);
    lv_anim_start(&a);
}

void ui_splash_create(lv_obj_t *parent, std::function<void()> on_done)
{
    s_on_done = std::move(on_done);

    // Full-screen overlay
    lv_obj_t *splash = lv_obj_create(parent);
    lv_obj_remove_style_all(splash);
    lv_obj_set_size(splash, LV_PCT(100), LV_PCT(100));

    // Deep gradient background
    lv_obj_set_style_bg_opa(splash, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(splash, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_bg_grad_color(splash, lv_color_hex(0x1a0a2e), 0);
    lv_obj_set_style_bg_grad_dir(splash, LV_GRAD_DIR_VER, 0);

    // Title
    lv_obj_t *title = lv_label_create(splash);
    lv_label_set_text(title, "Tab5");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_letter_space(title, 8, 0);
    lv_obj_set_style_opa(title, LV_OPA_TRANSP, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -30);

    // Subtitle
    lv_obj_t *sub = lv_label_create(splash);
    lv_label_set_text(sub, "companion");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x6666aa), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_letter_space(sub, 6, 0);
    lv_obj_set_style_opa(sub, LV_OPA_TRANSP, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 24);

    // Accent line
    lv_obj_t *line = lv_obj_create(splash);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, 0, 2);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x8844cc), 0);
    lv_obj_set_style_radius(line, 1, 0);
    lv_obj_align(line, LV_ALIGN_CENTER, 0, 58);

    // --- Animations ---

    // Title fade in
    lv_anim_t a_title;
    lv_anim_init(&a_title);
    lv_anim_set_var(&a_title, title);
    lv_anim_set_values(&a_title, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a_title, 800);
    lv_anim_set_delay(&a_title, 200);
    lv_anim_set_exec_cb(&a_title, [](void *obj, int32_t v) {
        lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), v, 0);
    });
    lv_anim_start(&a_title);

    // Subtitle fade in (delayed)
    lv_anim_t a_sub;
    lv_anim_init(&a_sub);
    lv_anim_set_var(&a_sub, sub);
    lv_anim_set_values(&a_sub, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a_sub, 800);
    lv_anim_set_delay(&a_sub, 600);
    lv_anim_set_exec_cb(&a_sub, [](void *obj, int32_t v) {
        lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), v, 0);
    });
    lv_anim_start(&a_sub);

    // Line expand
    lv_anim_t a_line;
    lv_anim_init(&a_line);
    lv_anim_set_var(&a_line, line);
    lv_anim_set_values(&a_line, 0, 200);
    lv_anim_set_duration(&a_line, 700);
    lv_anim_set_delay(&a_line, 900);
    lv_anim_set_path_cb(&a_line, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a_line, [](void *obj, int32_t v) {
        lv_obj_set_width(static_cast<lv_obj_t *>(obj), v);
    });
    lv_anim_start(&a_line);

    // Auto-exit after 2.5s
    lv_timer_create(trigger_exit, 2500, splash);
}
