#ifndef SM64_CHARACTER_WHEEL_H
#define SM64_CHARACTER_WHEEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
union SDL_Event;
/* Game-thread only. F7 is reserved only in --character-wheel mode. */
int character_wheel_handle_event(const union SDL_Event *event);
void character_wheel_update(void);
/* Polled SDL-standard controls: hold Back/Share or touchpad, aim left stick,
 * release to select; Circle/B cancels. Disconnect/focus/menu cancels safely. */
void character_wheel_gamepad(int connected,int allowed,int hold,int cancel,int16_t x,int16_t y);
void character_wheel_render(int framebuffer_width,int framebuffer_height);
void character_wheel_shutdown(void);
int character_wheel_is_open(void);
int character_wheel_blocks_gameplay(void);
/* Online selection/focus loss blocks only local input, never world/NAT ticks. */
int character_wheel_pauses_world(void);
int character_wheel_owns_pointer(void);
uint32_t character_wheel_filter_mouse(uint32_t buttons);
void character_wheel_filter_input(uint16_t *buttons,int8_t *x,int8_t *y,int8_t *cx,int8_t *cy);
/* Pure geometry: clockwise slots from top; center/outside/nonfinite cancels.
 * x/y are coordinates in the same pixel space as width/height. */
int character_wheel_pick(float x,float y,int width,int height);
typedef struct CharacterWheelSnapshot {
    int open,hovered,window_active,active;
    uint32_t opens,commits,cancels;
    char status[256];
} CharacterWheelSnapshot;
int character_wheel_snapshot(CharacterWheelSnapshot *out);
#ifdef __cplusplus
}
#endif
#endif
