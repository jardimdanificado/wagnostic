#ifndef PIOLHO_GAMEPAD_H
#define PIOLHO_GAMEPAD_H

#include <stdint.h>

#define WGAMEPAD_EXTENSION "std:gamepad"

/* Gamepad Buttons */
#define WGAMEPAD_BTN_A             (1 << 0)
#define WGAMEPAD_BTN_B             (1 << 1)
#define WGAMEPAD_BTN_X             (1 << 2)
#define WGAMEPAD_BTN_Y             (1 << 3)
#define WGAMEPAD_BTN_LEFTSHOULDER  (1 << 4)
#define WGAMEPAD_BTN_RIGHTSHOULDER (1 << 5)
#define WGAMEPAD_BTN_SELECT        (1 << 6)
#define WGAMEPAD_BTN_START         (1 << 7)
#define WGAMEPAD_BTN_LEFTSTICK     (1 << 8)
#define WGAMEPAD_BTN_RIGHTSTICK    (1 << 9)
#define WGAMEPAD_BTN_DPAD_UP       (1 << 10)
#define WGAMEPAD_BTN_DPAD_DOWN     (1 << 11)
#define WGAMEPAD_BTN_DPAD_LEFT     (1 << 12)
#define WGAMEPAD_BTN_DPAD_RIGHT    (1 << 13)

typedef struct {
    uint32_t buttons;  /* Offset  0 (4B) - Gamepad buttons bitmask */
    int16_t  axes[8];  /* Offset  4 (16B) - 8 analog axes (-32768..32767) */
} wgamepad_t;

#endif /* PIOLHO_GAMEPAD_H */
