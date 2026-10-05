#include "djui.h"
#include "djui_rocket_boost.h"
#include "pc/rocket_boost.h"
#include "pc/rocket_audio.h"
#include "pc/configfile.h"

static void sound_changed(struct DjuiBase *base) {
    (void)base; rocket_audio_stop(); configfile_save(configfile_name());
}

static unsigned speedSelection;
static void speed_refresh(struct DjuiBase *base, UNUSED bool *unused) {
    speedSelection = rocket_speed_percent();
    djui_slider_update_value(base);
    djui_base_set_enabled(base, rocket_boost_can_set_mode());
}
static void speed_changed(struct DjuiBase *base) {
    rocket_speed_set_percent(speedSelection);
    speed_refresh(base, NULL);
}
static void speed_scope(struct DjuiBase *base, UNUSED bool *unused) {
    djui_text_set_text((struct DjuiText *)base, rocket_speed_scope_label());
}
static unsigned selection;
static unsigned surfaceSelection;
static void surface_refresh(struct DjuiBase *base, UNUSED bool *unused) {
    surfaceSelection = rocket_surface_mode();
    djui_selectionbox_update_value(base);
    djui_base_set_enabled(base, rocket_boost_can_set_mode());
}
static void surface_changed(struct DjuiBase *base) {
    rocket_surface_set_mode(surfaceSelection);
    surface_refresh(base, NULL);
}
static void surface_scope(struct DjuiBase *base, UNUSED bool *unused) {
    djui_text_set_text((struct DjuiText *)base, rocket_surface_scope_label());
}
static void refresh(struct DjuiBase *base, UNUSED bool *unused) {
    selection = rocket_boost_mode();
    djui_selectionbox_update_value(base);
    djui_base_set_enabled(base, rocket_boost_can_set_mode());
}
static void changed(struct DjuiBase *base) {
    rocket_boost_set_mode(selection);
    refresh(base, NULL);
}
static void scope(struct DjuiBase *base, UNUSED bool *unused) {
    djui_text_set_text((struct DjuiText *)base, rocket_boost_scope_label());
}
void djui_rocket_boost_create(struct DjuiBase *parent) {
    speedSelection = rocket_speed_percent();
    struct DjuiSlider *slider = djui_slider_create(parent, "Octane speed (%)", &speedSelection,
        ROCKET_SPEED_MIN, ROCKET_SPEED_MAX, speed_changed);
    slider->base.on_render_pre = speed_refresh;
    speed_refresh(&slider->base, NULL);
    struct DjuiRect *speedRow = djui_rect_container_create(parent, 96);
    struct DjuiText *speedText = djui_text_create(&speedRow->base, rocket_speed_scope_label());
    djui_base_set_size_type(&speedText->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&speedText->base, 1, 96);
    speedText->base.on_render_pre = speed_scope;
    char *sounds[] = { "Mario", "Car (local Rocket League)" };
    djui_selectionbox_create(parent, "Octane sounds", sounds, 2, &configRocketSoundMode, sound_changed);
    char *choices[] = { "Coin only", "Infinite" };
    selection = rocket_boost_mode();
    struct DjuiSelectionbox *box = djui_selectionbox_create(parent, "Octane boost", choices, 2, &selection, changed);
    box->base.on_render_pre = refresh;
    refresh(&box->base, NULL);
    struct DjuiRect *row = djui_rect_container_create(parent, 64);
    struct DjuiText *text = djui_text_create(&row->base, rocket_boost_scope_label());
    djui_base_set_size_type(&text->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&text->base, 1, 64);
    text->base.on_render_pre = scope;
    char *surfaces[] = { "Car grip", "Native surfaces" };
    surfaceSelection = rocket_surface_mode();
    box = djui_selectionbox_create(parent, "Octane surfaces", surfaces, 2, &surfaceSelection, surface_changed);
    box->base.on_render_pre = surface_refresh;
    surface_refresh(&box->base, NULL);
    row = djui_rect_container_create(parent, 96);
    text = djui_text_create(&row->base, rocket_surface_scope_label());
    djui_base_set_size_type(&text->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&text->base, 1, 96);
    text->base.on_render_pre = surface_scope;
}
