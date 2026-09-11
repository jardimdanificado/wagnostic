#ifndef PIOLHO_GAMEPAD_H
#define PIOLHO_GAMEPAD_H

#include <stdint.h>

#define GAMEPAD_EXTENSION  "std:gamepad"
#define WGAMEPAD_EXTENSION "std:gamepad"

#define GAMEPAD_BTN_A             (1 << 0)
#define GAMEPAD_BTN_B             (1 << 1)
#define GAMEPAD_BTN_X             (1 << 2)
#define GAMEPAD_BTN_Y             (1 << 3)
#define GAMEPAD_BTN_LEFTSHOULDER  (1 << 4)
#define GAMEPAD_BTN_RIGHTSHOULDER (1 << 5)
#define GAMEPAD_BTN_SELECT        (1 << 6)
#define GAMEPAD_BTN_START         (1 << 7)
#define GAMEPAD_BTN_LEFTSTICK     (1 << 8)
#define GAMEPAD_BTN_RIGHTSTICK    (1 << 9)
#define GAMEPAD_BTN_DPAD_UP       (1 << 10)
#define GAMEPAD_BTN_DPAD_DOWN     (1 << 11)
#define GAMEPAD_BTN_DPAD_LEFT     (1 << 12)
#define GAMEPAD_BTN_DPAD_RIGHT    (1 << 13)

#define WGAMEPAD_BTN_A             GAMEPAD_BTN_A
#define WGAMEPAD_BTN_B             GAMEPAD_BTN_B
#define WGAMEPAD_BTN_X             GAMEPAD_BTN_X
#define WGAMEPAD_BTN_Y             GAMEPAD_BTN_Y
#define WGAMEPAD_BTN_LEFTSHOULDER  GAMEPAD_BTN_LEFTSHOULDER
#define WGAMEPAD_BTN_RIGHTSHOULDER GAMEPAD_BTN_RIGHTSHOULDER
#define WGAMEPAD_BTN_SELECT        GAMEPAD_BTN_SELECT
#define WGAMEPAD_BTN_START         GAMEPAD_BTN_START
#define WGAMEPAD_BTN_LEFTSTICK     GAMEPAD_BTN_LEFTSTICK
#define WGAMEPAD_BTN_RIGHTSTICK    GAMEPAD_BTN_RIGHTSTICK
#define WGAMEPAD_BTN_DPAD_UP       GAMEPAD_BTN_DPAD_UP
#define WGAMEPAD_BTN_DPAD_DOWN     GAMEPAD_BTN_DPAD_DOWN
#define WGAMEPAD_BTN_DPAD_LEFT     GAMEPAD_BTN_DPAD_LEFT
#define WGAMEPAD_BTN_DPAD_RIGHT    GAMEPAD_BTN_DPAD_RIGHT

typedef struct {
    uint32_t buttons;  /* Gamepad buttons bitmask */
    int16_t  axes[8];  /* 8 analog axis channels (-32768 to 32767) */
} gamepad_t;

typedef gamepad_t wgamepad_t;

#endif /* PIOLHO_GAMEPAD_H */
