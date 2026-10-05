#ifndef SMN64_TRAP_SHEET_H
#define SMN64_TRAP_SHEET_H
#include <stdint.h>
#include "../markers/markers_n64.h"
/* Original B07F0/B0A6C/B096C/B0B88 sheet sampling owner. Mesh contact decides
 * success and therefore future shared-RNG consumption. No synthetic markers.
 * Callback errors are fail-stop/whole-owner rollback boundaries: preceding
 * samples or RNG draws may have committed. Never retry just the local struct. */
typedef struct SmN64TrapSheet {
    uint32_t actor,segments,patch_count;
    int32_t height,radius,angle;
    uint8_t mode,interval,web_type;
    int8_t height_step;
    SmN64Marker markers[81];
    int16_t heights[81]; /*source422 is LOW half of height420, not angle428*/
    uint8_t patches[20][3];
} SmN64TrapSheet;
typedef struct SmN64TrapActor {
    int32_t position[3];
    uint8_t above,below; /*source unsigned actor142/143 native extents*/
} SmN64TrapActor;
typedef struct SmN64TrapSheetHost {
    void *context;
    /* Original B000C query: actor mesh bounds/joint poses, from outside ring
     * toward actor axis at the current sample height; source scale4096.
     * Return1 actual intersection and local joint marker,0miss,-1unavailable. */
    int (*mesh_marker)(void *,uint32_t actor,const int32_t from[3],
                       const int32_t to[3],int32_t scale,SmN64Marker *);
    /* Synchronous patch graphical allocation at B0CD4, texture35023093.
     * Source constructors have no explicit RNG call; texture resource loading
     * remains host-owned. Return exactly1, and never reroll sampling in drawing. */
    int (*patch)(void *,uint32_t actor,const uint8_t indices[3]);
} SmN64TrapSheetHost;
/* Fresh zeroed allocation semantics from6A94C. mode0trap or1yank only; other
 * source modes depend on an undefined callee-saved value and are rejected. */
int smn64_trap_sheet_init(SmN64TrapSheet *,uint32_t actor,uint8_t mode,
                        uint8_t web_type,const SmN64TrapActor *,uint32_t rng[3]);
int smn64_trap_sheet_advance(SmN64TrapSheet *,const SmN64TrapActor *,uint32_t rng[3]);
int smn64_trap_sheet_sample(SmN64TrapSheet *,const SmN64TrapActor *,uint32_t rng[3],
                          const SmN64TrapSheetHost *);
/* FireWeb B4168 requests exactly eight samples on the actor's existing sheet. */
int smn64_trap_sheet_feed(SmN64TrapSheet *,const SmN64TrapActor *,uint32_t rng[3],
                        const SmN64TrapSheetHost *);
typedef struct SmN64TrapColorState { uint16_t phase;int16_t blend; } SmN64TrapColorState;
typedef struct SmN64TrapColors {
    uint8_t points[81][3],patches[20][3];
    uint32_t actor_color,actor_set_flag; /*set_flag applies to actor0 halfword*/
} SmN64TrapColors;
/* Original B0FD0 after pose refresh: ordinary point greys consume RNG too.
 * Special sheet script8008A5E8 is a synchronous dependency between point and
 * patch random draws; selected_endpoint is a segment index, not marker index.
 * Pass actor raw word0 and byte23, preserving the source alpha eligibility. */
typedef int (*SmN64TrapSpark)(void *,uint32_t selected_endpoint,uint32_t rng[3]);
int smn64_trap_sheet_colors(const SmN64TrapSheet *,SmN64TrapColorState *,
    uint32_t actor_word0,uint8_t actor_byte23,uint32_t rng[3],
    SmN64TrapSpark,void *context,SmN64TrapColors *);
typedef struct SmN64SheetDebrisRandom { int32_t y_velocity_offset[3];uint8_t size_class; } SmN64SheetDebrisRandom;
/* Original B2884 mode0 release fragment draws, after geometry construction.
 * B0D30 creates ceil((segments-1)/2) fragments (at most40), each with these
 * FIVE draws. World marker refresh, floor ray and actual allocations precede it. */
int smn64_trap_sheet_debris_random(uint32_t count,uint32_t rng[3],SmN64SheetDebrisRandom out[40]);
#endif
