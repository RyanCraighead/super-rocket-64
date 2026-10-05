#ifndef SM64_BK_ADAPTER_H
#define SM64_BK_ADAPTER_H
struct MarioState;
/* Opt-in offline source-derived Banjo-Kazooie slice; native terrain/health
 * adapter. Unsupported scripted, swimming and held-object actions fall back. */
int bk_adapter_update(struct MarioState *m);
void bk_adapter_suspend(void);
/* Wheel ownership gate; ordinary launchers retain the default selected state. */
void bk_adapter_set_selected(int selected);
#endif
