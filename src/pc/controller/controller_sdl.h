#ifndef CONTROLLER_SDL_H
#define CONTROLLER_SDL_H

#include "controller_api.h"

#define VK_BASE_SDL_GAMEPAD 0x1000

// mouse buttons are also in the controller namespace, just offset 0x100
#define VK_OFS_SDL_MOUSE 0x0100
#define VK_BASE_SDL_MOUSE (VK_BASE_SDL_GAMEPAD + VK_OFS_SDL_MOUSE)
#define MWHEELUP    0x20
#define MWHEELDOWN  0x40

extern struct ControllerAPI controller_sdl;
void controller_sdl_set_window_active(int active);
int controller_sdl_rocket_input_blocked(void);
void controller_sdl_rocket_bindings_changed(void);
enum ControllerPromptDevice {
    CONTROLLER_PROMPT_KEYBOARD, CONTROLLER_PROMPT_XBOX, CONTROLLER_PROMPT_PLAYSTATION,
    CONTROLLER_PROMPT_NINTENDO, CONTROLLER_PROMPT_GENERIC
};
int controller_sdl_prompt_device(void);
void controller_sdl_note_keyboard_input(void);

#endif
