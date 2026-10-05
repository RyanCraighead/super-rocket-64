/* Uses the reviewed cloud SDL/real-framebuffer observer on native Windows.
 * Explicit QA build plus THPS_SCORE_SCENARIO are both required. */
#ifdef ROCKET_CAR_QA
#define THPS_BUILTIN_QA 1
#include "../../codex/thps/native/controller_observer.c"
#endif
