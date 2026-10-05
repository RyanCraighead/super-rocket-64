#ifndef SM64_OOT_LINK_ADAPTER_H
#define SM64_OOT_LINK_ADAPTER_H
struct MarioState;
/* Return 1 to skip native action dispatch, preserving the common Mario tail.
 * Supported: ordinary dry ground, free fall/autojump, one-handed sword.
 * Other actions remain native SM64. Local offline player only. */
int oot_link_adapter_update(struct MarioState *m);
void oot_link_adapter_suspend(void);
/* Wheel ownership gate; ordinary launchers retain the default selected state. */
void oot_link_adapter_set_selected(int selected);
#endif
