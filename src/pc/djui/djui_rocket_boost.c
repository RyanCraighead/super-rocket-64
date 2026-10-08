#include "djui.h"
#include "djui_rocket_boost.h"
#include "pc/rocket_boost.h"
#include "pc/rocket_audio.h"
#include "pc/configfile.h"

static void sound_changed(struct DjuiBase *base) {
    (void)base; rocket_audio_stop(); configfile_save(configfile_name());
}

static void scope_measure(struct DjuiBase *base, f32 width) {
    struct DjuiText *text = (struct DjuiText*)base->child->base;
    base->height.value = fmaxf(32, djui_text_measure_height(text, width));
    text->base.height.value = base->height.value;
}

static unsigned difficultySelection;
static void difficulty_refresh(struct DjuiBase *base, UNUSED bool *unused) {
    difficultySelection=rocket_difficulty();
    djui_selectionbox_update_value(base);
    djui_base_set_enabled(base,rocket_boost_can_set_mode());
}
static void difficulty_changed(struct DjuiBase *base) {
    /* Custom describes the sliders; it is not an applicable preset. The
     * controller always advances, so stopping on Custom trapped it at Hard.
     * Skip that display-only slot in either direction around the preset ring. */
    if (difficultySelection == ROCKET_CUSTOM) {
        unsigned previous = rocket_difficulty();
        if (previous == ROCKET_HARD) difficultySelection = ROCKET_EASY;
        else if (previous == ROCKET_EASY) difficultySelection = ROCKET_HARD;
    }
    rocket_difficulty_set(difficultySelection);
    difficulty_refresh(base,NULL);
}
static void difficulty_scope(struct DjuiBase *base, UNUSED bool *unused) {
    djui_text_set_text((struct DjuiText *)base,rocket_difficulty_scope_label());
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
static unsigned jumpSelection;
static void jump_refresh(struct DjuiBase *base, UNUSED bool *unused) {
    jumpSelection = rocket_jump_percent();
    djui_slider_update_value(base);
    djui_base_set_enabled(base, rocket_boost_can_set_mode());
}
static void jump_changed(struct DjuiBase *base) {
    rocket_jump_set_percent(jumpSelection);
    jump_refresh(base, NULL);
}
static void jump_scope(struct DjuiBase *base, UNUSED bool *unused) {
    djui_text_set_text((struct DjuiText *)base, rocket_jump_scope_label());
}
static unsigned selection;
static unsigned surfaceSelection;
/* Display the new default first without changing persisted/wire values 0/1. */
static void surface_refresh(struct DjuiBase *base, UNUSED bool *unused) {
    surfaceSelection = 2 - rocket_surface_mode();
    djui_selectionbox_update_value(base);
    djui_base_set_enabled(base, rocket_boost_can_set_mode());
}
static void surface_changed(struct DjuiBase *base) {
    if (surfaceSelection < 3) rocket_surface_set_mode(2 - surfaceSelection);
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
    char *presets[]={"Easy (100% / 100%)","Medium (75% / 50%)","Hard (50% / 30%)","Custom (use sliders)"};
    difficultySelection=rocket_difficulty();
    struct DjuiSelectionbox *difficulty=djui_selectionbox_create(parent,"Octane difficulty",presets,4,&difficultySelection,difficulty_changed);
    difficulty->base.on_render_pre=difficulty_refresh;
    difficulty_refresh(&difficulty->base,NULL);
    struct DjuiRect *difficultyRow=djui_rect_container_create(parent,96);
    difficultyRow->base.measure=scope_measure;
    struct DjuiText *difficultyText=djui_text_create(&difficultyRow->base,rocket_difficulty_scope_label());
    djui_base_set_size_type(&difficultyText->base,DJUI_SVT_RELATIVE,DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&difficultyText->base,1,96);
    difficultyText->base.on_render_pre=difficulty_scope;
    speedSelection = rocket_speed_percent();
    struct DjuiSlider *slider = djui_slider_create(parent, "Octane speed (%)", &speedSelection,
        ROCKET_SPEED_MIN, ROCKET_SPEED_MAX, speed_changed);
    slider->base.on_render_pre = speed_refresh;
    speed_refresh(&slider->base, NULL);
    struct DjuiRect *speedRow = djui_rect_container_create(parent, 96);
    speedRow->base.measure=scope_measure;
    struct DjuiText *speedText = djui_text_create(&speedRow->base, rocket_speed_scope_label());
    djui_base_set_size_type(&speedText->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&speedText->base, 1, 96);
    speedText->base.on_render_pre = speed_scope;
    jumpSelection = rocket_jump_percent();
    struct DjuiSlider *jumpSlider = djui_slider_create(parent, "Octane jump height (%)", &jumpSelection,
        ROCKET_JUMP_MIN, ROCKET_JUMP_MAX, jump_changed);
    jumpSlider->base.on_render_pre = jump_refresh;
    jump_refresh(&jumpSlider->base, NULL);
    struct DjuiRect *jumpRow = djui_rect_container_create(parent, 96);
    jumpRow->base.measure=scope_measure;
    struct DjuiText *jumpText = djui_text_create(&jumpRow->base, rocket_jump_scope_label());
    djui_base_set_size_type(&jumpText->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&jumpText->base, 1, 96);
    jumpText->base.on_render_pre = jump_scope;
    char *sounds[] = { "Mario", "Car (local Rocket League)" };
    djui_selectionbox_create(parent, "Octane sounds", sounds, 2, &configRocketSoundMode, sound_changed);
    char *choices[] = { "Coin only", "Infinite" };
    selection = rocket_boost_mode();
    struct DjuiSelectionbox *box = djui_selectionbox_create(parent, "Octane boost", choices, 2, &selection, changed);
    box->base.on_render_pre = refresh;
    refresh(&box->base, NULL);
    struct DjuiRect *row = djui_rect_container_create(parent, 64);
    row->base.measure=scope_measure;
    struct DjuiText *text = djui_text_create(&row->base, rocket_boost_scope_label());
    djui_base_set_size_type(&text->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&text->base, 1, 64);
    text->base.on_render_pre = scope;
    char *surfaces[] = { "Native: walls off", "Native: walls on", "Octane" };
    surfaceSelection = 2 - rocket_surface_mode();
    box = djui_selectionbox_create(parent, "Octane surfaces", surfaces, 3, &surfaceSelection, surface_changed);
    box->base.on_render_pre = surface_refresh;
    surface_refresh(&box->base, NULL);
    row = djui_rect_container_create(parent, 96);
    row->base.measure=scope_measure;
    text = djui_text_create(&row->base, rocket_surface_scope_label());
    djui_base_set_size_type(&text->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&text->base, 1, 96);
    text->base.on_render_pre = surface_scope;
}
