#ifndef CODEX_BM64_THROW_INPUT_H
#define CODEX_BM64_THROW_INPUT_H

/* Original 8024C884 selector; consume post-deadzone stick magnitude at the
 * frame12 release point, not the magnitude when B was first pressed.
 * scheme1 uses cumulative B-held updates since throw action began instead.
 * Normal B throws select only modes1..3. Mode0 is never the idle-B default.
 */
int bm64_throw_mode(float magnitude, unsigned held_b_updates, int scheme);

/* One-shot release predicate. The original action increments held-B count
 * first, tests channel0 animation frame exactly12, then sets released latch.
 * The host owns action/animation scheduling, so this has no hidden timer.
 */
int bm64_throw_releases(float animation_frame, int already_released);
#endif
