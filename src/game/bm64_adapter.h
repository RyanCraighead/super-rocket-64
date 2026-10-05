#ifndef SM64_BM64_ADAPTER_H
#define SM64_BM64_ADAPTER_H
struct MarioState;
/* Experimental offline adapter: source normal movement/bomb scalar kernels,
 * SM64 terrain/interaction contacts. Unsupported native actions return to SM64. */
int bm64_adapter_update(struct MarioState *m);
void bm64_adapter_suspend(void);
/* Wheel ownership gate; ordinary launchers retain the default selected state. */
void bm64_adapter_set_selected(int selected);
#endif
