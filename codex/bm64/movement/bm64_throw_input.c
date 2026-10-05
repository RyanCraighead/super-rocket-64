#include "bm64_throw_input.h"

int bm64_throw_mode(float magnitude, unsigned held_b_updates, int scheme) {
    /* 8024C9EC..CA68. Intended for normal player; special flag0x00080000
     * substitutes Player+0x80's preset and bypasses duration selection. */
    if (scheme == 1) {
        if (held_b_updates < 6u) return 1;
        if (held_b_updates < 13u) return 2;
        return 3;
    }
    /* 8024CA6C..CAD4. Caller supplies a valid nonnegative magnitude. */
    if (magnitude < 10.0f) return 1;
    if (magnitude < 60.0f) return 2;
    return 3;
}

int bm64_throw_releases(float animation_frame, int already_released) {
    /* 8024C944..968. Deliberately equality, not >= or a guessed delay. */
    return animation_frame == 12.0f && !already_released;
}
