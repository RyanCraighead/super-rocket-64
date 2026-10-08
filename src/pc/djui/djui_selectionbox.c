#include <stdio.h>
#include <string.h>
#include "djui.h"

extern ALIGNED8 u8 texture_selectionbox_back_icon[];
extern ALIGNED8 u8 texture_selectionbox_forward_icon[];

static void djui_selectionbox_update_style(struct DjuiBase* base) {
    struct DjuiSelectionbox* selectionbox = (struct DjuiSelectionbox*)base;
    struct DjuiTheme* theme = gDjuiThemes[configDjuiTheme];
    f32 x = selectionbox->rect->base.elem.x;
    bool activeRegion = (gCursorX >= x);

    if (!selectionbox->base.enabled) {
        struct DjuiSelectionbox* selectionbox = (struct DjuiSelectionbox*)base;
        struct DjuiColor bc = djui_theme_shade_color(theme->interactables.defaultBorderColor, 0.6f);
        struct DjuiColor rc = djui_theme_shade_color(theme->interactables.defaultRectColor, 0.6f);
        struct DjuiColor tc = theme->interactables.textColor;

        djui_base_set_border_color(&selectionbox->rect->base, bc.r, bc.g, bc.b, bc.a);
        djui_base_set_color(&selectionbox->rect->base, rc.r, rc.g, rc.b, rc.a);
        djui_base_set_color(&selectionbox->rectText->base, tc.r, tc.g, tc.b, tc.a);

        djui_base_set_location(&selectionbox->rectText->base, 0.0f, 3.0f);
        djui_base_set_color(&selectionbox->rectImage->base, tc.r, tc.g, tc.b, tc.a);
        djui_base_set_color(&selectionbox->rectImage2->base, tc.r, tc.g, tc.b, tc.a);
        djui_base_set_color(&selectionbox->text->base, 220, 220, 220, 255);
    } else if (gDjuiCursorDownOn == base && activeRegion) {
        struct DjuiColor bc = theme->interactables.cursorDownBorderColor;
        struct DjuiColor rc = theme->interactables.cursorDownRectColor;

        djui_base_set_border_color(&selectionbox->rect->base, bc.r, bc.g, bc.b, bc.a);
        djui_base_set_color(&selectionbox->rect->base, rc.r, rc.g, rc.b, rc.a);

        djui_base_set_location(&selectionbox->rectText->base, 0.5f, 3.5f);
        djui_base_set_color(&selectionbox->text->base, 220, 220, 220, 255);
    } else if (gDjuiHovered == base && activeRegion) {
        struct DjuiColor bc = theme->interactables.hoveredBorderColor;
        struct DjuiColor rc = theme->interactables.hoveredRectColor;

        djui_base_set_border_color(&selectionbox->rect->base, bc.r, bc.g, bc.b, bc.a);
        djui_base_set_color(&selectionbox->rect->base, rc.r, rc.g, rc.b, rc.a);

        djui_base_set_location(&selectionbox->rectText->base, -1.0f, 2.0f);
        djui_base_set_color(&selectionbox->text->base, 220, 220, 220, 255);
    } else {
        struct DjuiSelectionbox* selectionbox = (struct DjuiSelectionbox*)base;
        struct DjuiColor bc = theme->interactables.defaultBorderColor;
        struct DjuiColor rc = theme->interactables.defaultRectColor;
        struct DjuiColor tc = theme->interactables.textColor;

        djui_base_set_border_color(&selectionbox->rect->base, bc.r, bc.g, bc.b, bc.a);
        djui_base_set_color(&selectionbox->rect->base, rc.r, rc.g, rc.b, rc.a);
        djui_base_set_color(&selectionbox->rectText->base, tc.r, tc.g, tc.b, tc.a);

        djui_base_set_location(&selectionbox->rectText->base, 0.0f, 3.0f);
        djui_base_set_color(&selectionbox->rectImage->base, tc.r, tc.g, tc.b, tc.a);
        djui_base_set_color(&selectionbox->rectImage2->base, tc.r, tc.g, tc.b, tc.a);
        djui_base_set_color(&selectionbox->text->base, 220, 220, 220, 255);
    }
}

static void djui_selectionbox_get_cursor_hover_location(struct DjuiBase* base, f32* x, f32* y) {
    struct DjuiSelectionbox* selectionbox = (struct DjuiSelectionbox*)base;
    struct DjuiBase* rectBase = &selectionbox->rect->base;
    *x = (rectBase->elem.x + rectBase->elem.width  * 3.0f / 4.0f);
    *y = (rectBase->elem.y + rectBase->elem.height * 3.0f / 4.0f);
}

void djui_selectionbox_update_value(struct DjuiBase* base) {
    struct DjuiSelectionbox* selectionbox = (struct DjuiSelectionbox*)base;
    djui_text_set_text(selectionbox->rectText, selectionbox->choices[*selectionbox->value]);
}

static void djui_selectionbox_on_cursor_down_begin(struct DjuiBase* base, UNUSED bool inputCursor) {
    struct DjuiSelectionbox* selectionbox = (struct DjuiSelectionbox*)base;
    f32 x = selectionbox->rect->base.elem.x;
    if (gCursorX >= x) {
        if ((gCursorX - x) / selectionbox->rect->base.elem.width > 0.5f) {
            *selectionbox->value = (*selectionbox->value + 1) % selectionbox->choiceCount;
        } else {
            s32 value = *selectionbox->value - 1;
            *selectionbox->value = value < 0 ? selectionbox->choiceCount - 1 : value;
        }
        djui_selectionbox_update_value(base);
        if (base != NULL && base->interactable != NULL && base->interactable->on_value_change != NULL) {
            base->interactable->on_value_change(base);
        }
    }
}


static void djui_selectionbox_destroy(struct DjuiBase* base) {
    struct DjuiSelectionbox* selectionbox = (struct DjuiSelectionbox*)base;
    for (int i = 0; i < selectionbox->choiceCount; i++) {
        free(selectionbox->choices[i]);
    }
    free(selectionbox->choices);
    free(selectionbox);
}

static void djui_selectionbox_measure(struct DjuiBase* base, f32 width) {
    struct DjuiSelectionbox* box = (struct DjuiSelectionbox*)base;
    bool stacked = width < 360;
    f32 labelHeight = djui_text_measure_height(box->text, width * (stacked ? 1 : 0.50f));
    f32 valueHeight = 0;
    // Measure every choice so cycling values does not move the clicked row.
    struct DjuiText text = *box->rectText;
    for (int i = 0; i < box->choiceCount; i++) {
        text.message = box->choices[i];
        valueHeight = fmaxf(valueHeight, djui_text_measure_height(&text, width * (stacked ? 1 : 0.45f) - 40));
    }
    djui_base_set_size_type(&box->text->base, DJUI_SVT_RELATIVE, stacked ? DJUI_SVT_ABSOLUTE : DJUI_SVT_RELATIVE);
    djui_base_set_size(&box->text->base, stacked ? 1 : 0.50f, stacked ? labelHeight : 1);
    djui_base_set_alignment(&box->text->base, DJUI_HALIGN_LEFT, stacked ? DJUI_VALIGN_TOP : DJUI_VALIGN_CENTER);
    djui_base_set_size_type(&box->rect->base, DJUI_SVT_RELATIVE, stacked ? DJUI_SVT_ABSOLUTE : DJUI_SVT_RELATIVE);
    djui_base_set_size(&box->rect->base, stacked ? 1 : 0.45f, stacked ? valueHeight + 8 : 1);
    djui_base_set_location(&box->rect->base, 0, stacked ? labelHeight + 8 : 0);
    djui_base_set_alignment(&box->rect->base, DJUI_HALIGN_RIGHT, stacked ? DJUI_VALIGN_TOP : DJUI_VALIGN_CENTER);
    base->height.value = stacked ? labelHeight + valueHeight + 16 : fmaxf(32, fmaxf(labelHeight, valueHeight) + 8);
}

struct DjuiSelectionbox* djui_selectionbox_create(struct DjuiBase* parent, const char* message, char* choices[], u8 choiceCount, unsigned int* value, void (*on_value_change)(struct DjuiBase*)) {
    struct DjuiSelectionbox* selectionbox = calloc(1, sizeof(struct DjuiSelectionbox));
    struct DjuiBase* base = &selectionbox->base;

    if (*value >= choiceCount) {
        *value = choiceCount - 1;
    }

    selectionbox->value = value;
    selectionbox->choices = calloc(choiceCount, sizeof(char*));
    for (int i = 0; i < choiceCount; i++) {
        u32 length = strlen(choices[i]);
        selectionbox->choices[i] = calloc((length + 1), sizeof(char));
        sprintf(selectionbox->choices[i], "%s", choices[i]);
    }
    selectionbox->choiceCount = choiceCount;

    djui_base_init(parent, base, NULL, djui_selectionbox_destroy);
    base->measure = djui_selectionbox_measure;
    djui_interactable_create(base, djui_selectionbox_update_style);
    djui_interactable_hook_cursor_down(base, djui_selectionbox_on_cursor_down_begin, NULL, NULL);

    struct DjuiText* text = djui_text_create(&selectionbox->base, message);
    djui_base_set_alignment(&text->base, DJUI_HALIGN_LEFT, DJUI_VALIGN_CENTER);
    djui_base_set_size_type(&text->base, DJUI_SVT_RELATIVE, DJUI_SVT_RELATIVE);
    djui_base_set_size(&text->base, 0.50f, 1.0f);
    djui_text_set_alignment(text, DJUI_HALIGN_LEFT, DJUI_VALIGN_BOTTOM);
    djui_text_set_drop_shadow(text, 64, 64, 64, 100);
    selectionbox->text = text;

    struct DjuiRect* rect = djui_rect_create(&selectionbox->base);
    djui_base_set_alignment(&rect->base, DJUI_HALIGN_RIGHT, DJUI_VALIGN_CENTER);
    djui_base_set_size_type(&rect->base, DJUI_SVT_RELATIVE, DJUI_SVT_RELATIVE);
    djui_base_set_size(&rect->base, 0.45f, 1.0f);
    djui_base_set_color(&rect->base, 0, 0, 0, 0);
    djui_base_set_border_width(&rect->base, 2);
    djui_base_set_padding(&rect->base, 2, 18, 2, 18);
    selectionbox->rect = rect;

    struct DjuiText* rectText = djui_text_create(&rect->base, choices[*value]);
    djui_base_set_alignment(&rectText->base, DJUI_HALIGN_LEFT, DJUI_VALIGN_CENTER);
    djui_base_set_size_type(&rectText->base, DJUI_SVT_RELATIVE, DJUI_SVT_RELATIVE);
    djui_base_set_size(&rectText->base, 1.0f, 1.0f);
    djui_text_set_alignment(rectText, DJUI_HALIGN_CENTER, DJUI_VALIGN_BOTTOM);
    djui_text_set_drop_shadow(rectText, 64, 64, 64, 100);
    selectionbox->rectText = rectText;

    struct DjuiImage* rectImage = djui_image_create(&rect->base, texture_selectionbox_back_icon, 16, 16, G_IM_FMT_RGBA, G_IM_SIZ_16b);
    djui_base_set_location(&rectImage->base, -16, 0);
    djui_base_set_size(&rectImage->base, 16, 16);
    djui_base_set_alignment(&rectImage->base, DJUI_HALIGN_LEFT, DJUI_VALIGN_CENTER);
    selectionbox->rectImage = rectImage;

    struct DjuiImage* rectImage2 = djui_image_create(&rect->base, texture_selectionbox_forward_icon, 16, 16, G_IM_FMT_RGBA, G_IM_SIZ_16b);
    djui_base_set_location(&rectImage2->base, -16, 0);
    djui_base_set_size(&rectImage2->base, 16, 16);
    djui_base_set_alignment(&rectImage2->base, DJUI_HALIGN_RIGHT, DJUI_VALIGN_CENTER);
    selectionbox->rectImage2 = rectImage2;

    djui_selectionbox_update_style(base);

    base->get_cursor_hover_location = djui_selectionbox_get_cursor_hover_location;

    djui_base_set_size_type(base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(base, 1.0f, 32);
    djui_interactable_hook_value_change(base, on_value_change);

    return selectionbox;
}
