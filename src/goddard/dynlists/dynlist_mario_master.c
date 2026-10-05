#include "pc/rom_assets.h"
#include <PR/ultratypes.h>

#include "dynlist_macros.h"
#include "dynlists.h"
#include "animdata.h"
#include "../dynlist_proc.h"
#include "../shape_helper.h"

static const struct RomDynlistBinding dynlist_mario_master_bindings[] = {
    { 3, 1, 0x040031f0u, (void *)(dynlist_mario_face_shape) },
    { 4, 1, 0x040039d8u, (void *)(dynlist_mario_eye_right_shape) },
    { 5, 1, 0x04004040u, (void *)(dynlist_mario_eye_left_shape) },
    { 6, 1, 0x040044b4u, (void *)(dynlist_mario_eyebrow_right_shape) },
    { 7, 1, 0x04004808u, (void *)(dynlist_mario_eyebrow_left_shape) },
    { 8, 1, 0x04004e10u, (void *)(dynlist_mario_mustache_shape) },
    { 14, 1, 0x801a8488u, (void *)(&gShapeSilverStar) },
    { 18, 1, 0x801a8484u, (void *)(&gShapeRedStar) },
    { 873, 1, 0x0400c6dcu, (void *)(&anim_mario_mustache_right) },
    { 879, 1, 0x0400de1cu, (void *)(&anim_mario_mustache_left) },
    { 885, 1, 0x0400f55cu, (void *)(&anim_mario_lips_1) },
    { 891, 1, 0x04010c9cu, (void *)(&anim_mario_lips_2) },
    { 897, 1, 0x040123dcu, (void *)(&anim_mario_eyebrows_1) },
    { 903, 1, 0x04013738u, (void *)(&anim_mario_eyebrows_equalizer) },
    { 909, 1, 0x04014e78u, (void *)(&anim_mario_eyebrows_2) },
    { 915, 1, 0x040161d4u, (void *)(&anim_mario_eyebrows_3) },
    { 921, 1, 0x04017914u, (void *)(&anim_mario_eyebrows_4) },
    { 927, 1, 0x04018c70u, (void *)(&anim_mario_eyebrows_5) },
    { 933, 1, 0x0401a3b0u, (void *)(&anim_mario_eye_left) },
    { 939, 1, 0x0401baf0u, (void *)(&anim_mario_eye_right) },
    { 945, 1, 0x0401d230u, (void *)(&anim_mario_cap) },
    { 951, 1, 0x0401e970u, (void *)(&anim_mario_lips_3) },
    { 957, 1, 0x040200b0u, (void *)(&anim_mario_lips_4) },
    { 963, 1, 0x0402140cu, (void *)(&anim_mario_ear_left) },
    { 969, 1, 0x04022768u, (void *)(&anim_mario_ear_right) },
    { 975, 1, 0x04023ea8u, (void *)(&anim_mario_nose) },
    { 981, 1, 0x040255e8u, (void *)(&anim_mario_lips_5) },
    { 987, 1, 0x04026d28u, (void *)(&anim_mario_lips_6) },
    { 993, 1, 0x04028468u, (void *)(&anim_mario_eyelid_left) },
    { 999, 1, 0x04029ba8u, (void *)(&anim_mario_eyelid_right) },
    { 1005, 1, 0x0402ca04u, (void *)(&anim_mario_intro) },
    { 1011, 1, 0x0402f860u, (void *)(&anim_silver_star) },
    { 1017, 1, 0x040326bcu, (void *)(&anim_red_star) },
};

ROM_ASSET_LOAD_DYNLIST(dynlist_mario_master, 0x00278930, 24624, dynlist_mario_master_bindings, 33);
