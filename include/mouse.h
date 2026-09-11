#ifndef PIOLHO_MOUSE_H
#define PIOLHO_MOUSE_H

#include <stdint.h>

#define WMOUSE_EXTENSION "std:mouse"

/* Mouse Buttons */
#define WMOUSE_BTN_LEFT   (1 << 0)
#define WMOUSE_BTN_RIGHT  (1 << 1)
#define WMOUSE_BTN_MIDDLE (1 << 2)

typedef struct {
    int32_t  x;          /* Offset  0 (4B) - Cursor X coordinate */
    int32_t  y;          /* Offset  4 (4B) - Cursor Y coordinate */
    uint32_t buttons;    /* Offset  8 (4B) - Buttons bitmask (1=L, 2=R, 4=M) */
    int32_t  wheel_x;    /* Offset 12 (4B) - Horizontal scroll delta */
    int32_t  wheel_y;    /* Offset 16 (4B) - Vertical scroll delta */
} wmouse_t;

#endif /* PIOLHO_MOUSE_H */
