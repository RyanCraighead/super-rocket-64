#ifndef CHARACTER_PRESENTATION_H
#define CHARACTER_PRESENTATION_H
#include "pc/rocket_runtime.h"
struct MarioState;
/* Native actions keep their gameplay ownership. Only their visual proxy changes. */
void character_presentation_begin(struct MarioState *m);
void character_presentation_finish(struct MarioState *m, int source_owns_action);
int character_presentation_car_snapshot(RocketSnapshot *out);
void character_presentation_draw(const float *view, const float *projection, const int *viewport);
#endif
