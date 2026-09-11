#ifndef PIOLHO_KEYBOARD_H
#define PIOLHO_KEYBOARD_H

#include <stdint.h>

#define KEYBOARD_EXTENSION  "std:keyboard"
#define WKEYBOARD_EXTENSION "std:keyboard"

typedef struct {
    uint8_t keys[256]; /* 1 = Key Down, 0 = Key Up (Indexed by USB HID scancode) */
} keyboard_t;

typedef keyboard_t wkeyboard_t;

#endif /* PIOLHO_KEYBOARD_H */
