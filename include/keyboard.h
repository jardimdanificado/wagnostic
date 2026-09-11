#ifndef WAGNOSTIC_KEYBOARD_H
#define WAGNOSTIC_KEYBOARD_H

#include <stdint.h>

#define WKEYBOARD_EXTENSION "std:keyboard"

typedef struct {
    uint8_t keys[256]; /* Offset 0 (256B) - USB HID scancodes (0=up, 1=down) */
} wkeyboard_t;

#endif /* WAGNOSTIC_KEYBOARD_H */
