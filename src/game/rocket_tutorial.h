#ifndef ROCKET_TUTORIAL_H
#define ROCKET_TUTORIAL_H
#include <stddef.h>
#include <stdint.h>
#define ROCKET_TUTORIAL_CAPACITY 4096
#define ROCKET_TUTORIAL_LINE_WIDTH 128
/* Ephemeral English text for existing native triggers, never a saved dialog
 * or a mutation of the ROM/Lua dialog table. Zero means use the original. */
int rocket_tutorial_text(int dialog, int lines, uint8_t text[ROCKET_TUTORIAL_CAPACITY]);
int rocket_tutorial_page(const uint8_t *text, int position, int lines);
int rocket_tutorial_page_start(const uint8_t *text, int page, int lines);
#endif
