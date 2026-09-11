#ifndef PIOLHO_MOUSE_H
#define PIOLHO_MOUSE_H

#include <stdint.h>

#define MOUSE_EXTENSION  "std:mouse"
#define WMOUSE_EXTENSION "std:mouse"

#define MOUSE_BTN_LEFT    (1 << 0)
#define MOUSE_BTN_RIGHT   (1 << 1)
#define MOUSE_BTN_MIDDLE  (1 << 2)

#define WMOUSE_BTN_LEFT   MOUSE_BTN_LEFT
#define WMOUSE_BTN_RIGHT  MOUSE_BTN_RIGHT
#define WMOUSE_BTN_MIDDLE MOUSE_BTN_MIDDLE

typedef struct {
    int32_t  x;          /* Cursor X position in pixels */
    int32_t  y;          /* Cursor Y position in pixels */
    uint32_t buttons;    /* Active buttons bitmask */
    int32_t  wheel_x;    /* Horizontal scroll wheel delta */
    int32_t  wheel_y;    /* Vertical scroll wheel delta */
} mouse_t;

typedef mouse_t wmouse_t;

#endif /* PIOLHO_MOUSE_H */
