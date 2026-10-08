#include "djui.h"

  ////////////////
 // properties //
////////////////

void djui_flow_layout_set_flow_direction(struct DjuiFlowLayout* layout, enum DjuiFlowDirection flowDirection) {
    layout->flowDirection = flowDirection;
}

void djui_flow_layout_set_margin(struct DjuiFlowLayout* layout, f32 margin) {
    layout->margin.value = margin;
}

void djui_flow_layout_set_margin_type(struct DjuiFlowLayout* layout, enum DjuiScreenValueType marginType) {
    layout->margin.type = marginType;
}

  ////////////
 // events //
////////////

static void djui_flow_layout_on_child_render(struct DjuiBase* base, struct DjuiBase* child) {
    if (!child->visible) { return; }
    struct DjuiFlowLayout* layout = (struct DjuiFlowLayout*)base;
    switch (layout->flowDirection) {
        case DJUI_FLOW_DIR_DOWN:
            base->comp.y      += (child->elem.height + layout->margin.value);
            base->comp.height -= (child->elem.height + layout->margin.value);
            break;
        case DJUI_FLOW_DIR_UP:
            base->comp.height -= (child->elem.height + layout->margin.value);
            break;
        case DJUI_FLOW_DIR_RIGHT:
            base->comp.x     += (child->elem.width + layout->margin.value);
            base->comp.width -= (child->elem.width + layout->margin.value);
            break;
        case DJUI_FLOW_DIR_LEFT:
            base->comp.width -= (child->elem.width + layout->margin.value);
            break;
    }
}

static bool djui_flow_layout_render(struct DjuiBase* base);

static void djui_flow_layout_measure(struct DjuiBase* base, f32 width) {
    struct DjuiFlowLayout* layout = (struct DjuiFlowLayout*)base;
    if (layout->flowDirection != DJUI_FLOW_DIR_DOWN) { return; }
    f32 height = 0;
    for (struct DjuiBaseChild* child = base->child; child; child = child->next) {
        struct DjuiBase* row = child->base;
        if (!row->visible) { continue; }
        f32 rowWidth = row->width.type == DJUI_SVT_RELATIVE ? width * row->width.value : row->width.value;
        if (row->measure) { row->measure(row, rowWidth); }
        if (row->height.type != DJUI_SVT_ABSOLUTE) { return; }
        height += row->height.value + layout->margin.value;
    }
    base->height.value = fmaxf(0, height - layout->margin.value);
}

static struct DjuiFlowLayout* djui_flow_layout_scroll_parent(struct DjuiBase* base) {
    for (; base; base = base->parent) {
        if (base->render == djui_flow_layout_render && ((struct DjuiFlowLayout*)base)->scrollable) {
            return (struct DjuiFlowLayout*)base;
        }
    }
    return NULL;
}

void djui_flow_layout_reveal(struct DjuiBase* selected) {
    struct DjuiFlowLayout* layout = djui_flow_layout_scroll_parent(selected);
    if (layout) { layout->manualScroll = false; }
}

bool djui_flow_layout_scroll(struct DjuiBase* hovered, f32 amount) {
    struct DjuiFlowLayout* layout = djui_flow_layout_scroll_parent(hovered);
    if (!layout || layout->contentHeight <= layout->viewportHeight) { return false; }
    layout->scrollOffset = fmaxf(0, fminf(layout->contentHeight - layout->viewportHeight,
                                        layout->scrollOffset - amount * 64));
    layout->manualScroll = true;
    return true;
}

static void djui_flow_layout_wrap_check(struct DjuiBase* base, struct DjuiBase** pick, s8 direction, f32 x) {
    if (!base->visible || !base->enabled) { return; }
    if (base->interactable && base->interactable->enabled) {
        f32 y = base->elem.y;
        if (!*pick || (direction > 0 ? y < (*pick)->elem.y : y > (*pick)->elem.y)
            || (y == (*pick)->elem.y && fabsf(base->elem.x - x) < fabsf((*pick)->elem.x - x))) {
            *pick = base;
        }
    }
    for (struct DjuiBaseChild* child = base->child; child; child = child->next) {
        djui_flow_layout_wrap_check(child->base, pick, direction, x);
    }
}

struct DjuiBase* djui_flow_layout_wrap(struct DjuiBase* selected, s8 direction) {
    struct DjuiFlowLayout* layout = djui_flow_layout_scroll_parent(selected);
    if (!layout || !selected) { return NULL; }
    for (struct DjuiBase* parent = &layout->base; parent; parent = parent->parent) {
        if (!parent->visible || !parent->enabled) { return NULL; }
    }
    struct DjuiBase* pick = NULL;
    djui_flow_layout_wrap_check(&layout->base, &pick, direction, selected->elem.x);
    return pick;
}

// Lay out offscreen descendants too: keyboard/controller navigation uses their
// geometry, even when clipping prevents the renderer from visiting them.
static void djui_flow_layout_compute_children(struct DjuiBase* base) {
    djui_base_compute(base);
    struct DjuiBaseRect saved = base->comp;
    f32 border = base->borderColor.a ? base->borderWidth.value : 0;
    base->comp.x += border;
    base->comp.y += border;
    base->comp.width -= border * 2;
    base->comp.height -= border * 2;
    f32 left = base->padding.left.value * (base->padding.left.type == DJUI_SVT_RELATIVE ? base->comp.width : 1);
    f32 right = base->padding.right.value * (base->padding.right.type == DJUI_SVT_RELATIVE ? base->comp.width : 1);
    f32 top = base->padding.top.value * (base->padding.top.type == DJUI_SVT_RELATIVE ? base->comp.height : 1);
    f32 bottom = base->padding.bottom.value * (base->padding.bottom.type == DJUI_SVT_RELATIVE ? base->comp.height : 1);
    base->comp.x += left;
    base->comp.y += top;
    base->comp.width -= left + right;
    base->comp.height -= top + bottom;
    for (struct DjuiBaseChild* child = base->child; child; child = child->next) {
        if (!child->base->visible) { continue; }
        djui_flow_layout_compute_children(child->base);
        if (base->on_child_render == djui_flow_layout_on_child_render) {
            djui_flow_layout_on_child_render(base, child->base);
        }
    }
    base->comp = saved;
}

static bool djui_flow_layout_render(struct DjuiBase* base) {
    struct DjuiFlowLayout* layout = (struct DjuiFlowLayout*)base;
    djui_rect_render(base);
    if (!layout->scrollable || layout->flowDirection != DJUI_FLOW_DIR_DOWN) { return true; }

    // Keep the scrollbar outside the content so labels never paint over it.
    base->comp.width = fmaxf(1, base->comp.width - 16);
    layout->viewportHeight = base->comp.height;
    layout->contentHeight = 0;
    struct DjuiBase* selected = djui_cursor_input_controlled_get();
    struct DjuiBase* ancestor = selected;
    while (ancestor && ancestor->parent != base) { ancestor = ancestor->parent; }
    if (!ancestor) { selected = NULL; }
    for (struct DjuiBaseChild* child = base->child; child; child = child->next) {
        struct DjuiBase* row = child->base;
        if (!row->visible) { continue; }
        f32 width = row->width.type == DJUI_SVT_RELATIVE ? base->comp.width * row->width.value : row->width.value;
        if (row->measure) { row->measure(row, width); }
        layout->contentHeight += row->height.value + layout->margin.value;
    }
    layout->contentHeight = fmaxf(0, layout->contentHeight - layout->margin.value);
    struct DjuiBaseRect viewport = base->comp;
    base->comp.height = fmaxf(layout->viewportHeight, layout->contentHeight);
    for (struct DjuiBaseChild* child = base->child; child; child = child->next) {
        if (!child->base->visible) { continue; }
        djui_flow_layout_compute_children(child->base);
        djui_flow_layout_on_child_render(base, child->base);
    }
    base->comp = viewport;
    if (selected && !layout->manualScroll) {
        f32 selectedTop = selected->elem.y - viewport.y;
        f32 selectedBottom = selectedTop + selected->elem.height;
        if (selectedBottom > layout->scrollOffset + layout->viewportHeight) {
            layout->scrollOffset = selectedBottom - layout->viewportHeight;
        }
        if (selectedTop < layout->scrollOffset) { layout->scrollOffset = selectedTop; }
    }
    f32 maxScroll = fmaxf(0, layout->contentHeight - layout->viewportHeight);
    layout->scrollOffset = fmaxf(0, fminf(layout->scrollOffset, maxScroll));
    if (maxScroll > 0) {
        struct DjuiBase bar = { 0 };
        bar.clip = base->clip;
        bar.clip.x = base->comp.x + base->comp.width + 8;
        bar.clip.width = 6;
        bar.color = (struct DjuiColor){ 80, 80, 80, 160 };
        djui_rect_render(&bar);
        f32 thumb = fmaxf(20, layout->viewportHeight * layout->viewportHeight / layout->contentHeight);
        thumb = fminf(thumb, layout->viewportHeight);
        bar.clip.y += (layout->viewportHeight - thumb) * layout->scrollOffset / maxScroll;
        bar.clip.height = thumb;
        bar.color = (struct DjuiColor){ 220, 220, 220, 255 };
        djui_rect_render(&bar);
    }
    base->comp.y -= layout->scrollOffset;
    base->comp.height = fmaxf(layout->viewportHeight, layout->contentHeight);
    struct DjuiBaseRect saved = base->comp;
    for (struct DjuiBaseChild* child = base->child; child; child = child->next) {
        if (!child->base->visible) { continue; }
        djui_flow_layout_compute_children(child->base);
        djui_flow_layout_on_child_render(base, child->base);
    }
    base->comp = saved;
    return true;
}

static void djui_flow_layout_destroy(struct DjuiBase* base) {
    struct DjuiFlowLayout* layout = (struct DjuiFlowLayout*)base;
    free(layout);
}

struct DjuiFlowLayout* djui_flow_layout_create(struct DjuiBase* parent) {
    struct DjuiFlowLayout* layout = calloc(1, sizeof(struct DjuiFlowLayout));
    struct DjuiBase* base         = &layout->base;

    djui_base_init(parent, base, djui_flow_layout_render, djui_flow_layout_destroy);
    base->measure = djui_flow_layout_measure;
    djui_base_set_size(base, 256, 512);

    djui_flow_layout_set_flow_direction(layout, DJUI_FLOW_DIR_DOWN);
    djui_flow_layout_set_margin(layout, 8);

    layout->base.on_child_render = djui_flow_layout_on_child_render;
    return layout;
}
