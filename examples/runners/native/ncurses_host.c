#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <ncurses.h>

#include "wasm3.h"
#include "m3_env.h"

#pragma pack(push, 1)
typedef struct {
    char     message[128];      // Offset 0
    uint32_t width;             // Offset 128
    uint32_t height;            // Offset 132
    uint32_t bpp;               // Offset 136
    uint32_t scale;             // Offset 140
    uint32_t audio_size;        // Offset 144
    uint32_t audio_write;       // Offset 148
    uint32_t audio_read;        // Offset 152
    uint32_t audio_rate;        // Offset 156
    uint32_t audio_bpp;         // Offset 160
    uint32_t audio_channels;    // Offset 164
    uint32_t ticks;             // Offset 168
    uint32_t gamepad;           // Offset 172
    int32_t  jx, jy, rx, ry;    // Offset 176
    uint8_t  keys[256];         // Offset 192
    int32_t  mouse_x, mouse_y;  // Offset 448
    uint32_t mouse_buttons;     // Offset 456
    int32_t  mouse_wheel;       // Offset 460
    uint8_t  signals[4];        // Offset 464
    uint8_t  reserved[44];      // Offset 468 - 511
} SystemConfig;
#pragma pack(pop)

static IM3Runtime runtime = NULL;
static uint32_t W=320, H=240, BPP=8;

// ASCII Palette for brightness levels
static const char* ASCII_RAMP = " .:-=+*#%@";
static const int RAMP_SIZE = 10;

uint32_t get_ticks() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void update_system_config() {
    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    if (!mem) return;
    SystemConfig* sys = (SystemConfig*)mem;
    
    W = sys->width;
    H = sys->height;
    BPP = sys->bpp;
    
    if (W == 0) W = 320;
    if (H == 0) H = 240;
    if (BPP == 0) BPP = 8;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <file.wasm>\n", argv[0]);
        return 1;
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        perror("Failed to open file");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* wasm_data = malloc(sz);
    fread(wasm_data, 1, sz, f);
    fclose(f);

    IM3Environment env = m3_NewEnvironment();
    runtime = m3_NewRuntime(env, 64*1024*1024, NULL);
    IM3Module module;
    m3_ParseModule(env, &module, wasm_data, sz);
    m3_LoadModule(runtime, module);

    IM3Function f_init = NULL, f_upd = NULL;
    if (m3_FindFunction(&f_init, runtime, "winit") != m3Err_none) 
        m3_FindFunction(&f_init, runtime, "init");
    if (m3_FindFunction(&f_upd, runtime, "wupdate") != m3Err_none) 
        m3_FindFunction(&f_upd, runtime, "frame");

    if (f_init) m3_CallV(f_init);
    update_system_config();

    // Ncurses Setup
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    printf("\033[?1003h\n"); // Enable mouse movement reporting

    int running = 1;
    uint32_t start_ticks = get_ticks();
    uint8_t key_timers[256] = {0};
    uint8_t gamepad_timers[8] = {0};

    while (running) {
        uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
        if (!mem) break;
        SystemConfig* sys = (SystemConfig*)mem;
        sys->ticks = get_ticks() - start_ticks;

        // 1. Handle Persistence/Decay
        sys->gamepad = 0;
        for (int i = 0; i < 256; i++) {
            if (key_timers[i] > 0) {
                sys->keys[i] = 1;
                key_timers[i]--;
            } else {
                sys->keys[i] = 0;
            }
        }
        for (int i = 0; i < 8; i++) {
            if (gamepad_timers[i] > 0) {
                sys->gamepad |= (1 << i);
                gamepad_timers[i]--;
            }
        }

        // 2. Input handling
        int ch = getch();
        while (ch != ERR) {
            if (ch == 'q' || ch == 27) running = 0;
            
            // Set timers (approx 10 frames covers typical terminal repeat delay)
            const int HOLD = 10;

            // Gamepad mapping
            if (ch == KEY_UP)    gamepad_timers[0] = HOLD;
            if (ch == KEY_DOWN)  gamepad_timers[1] = HOLD;
            if (ch == KEY_LEFT)  gamepad_timers[2] = HOLD;
            if (ch == KEY_RIGHT) gamepad_timers[3] = HOLD;
            if (ch == 'z' || ch == 'Z') gamepad_timers[4] = HOLD; // A
            if (ch == 'x' || ch == 'X') gamepad_timers[5] = HOLD; // B
            if (ch == ' ')              gamepad_timers[6] = HOLD; // Select
            if (ch == 10 || ch == 13 || ch == KEY_ENTER) {
                gamepad_timers[7] = HOLD; // Start
                key_timers[40] = HOLD;    // Enter
            }

            // Keyboard mapping (HID Scancodes)
            if (ch == 27) key_timers[41] = HOLD; // ESC
            if (ch == KEY_BACKSPACE || ch == 127) key_timers[42] = HOLD;
            if (ch == '\t') key_timers[43] = HOLD;
            if (ch >= 'a' && ch <= 'z') key_timers[4 + (ch - 'a')] = HOLD;
            if (ch >= 'A' && ch <= 'Z') key_timers[4 + (ch - 'A')] = HOLD;
            if (ch >= '1' && ch <= '9') key_timers[30 + (ch - '1')] = HOLD;
            if (ch == '0') key_timers[39] = HOLD;
            if (ch == ' ') key_timers[44] = HOLD;

            // Mouse handling (Mouse events don't need decay)
            if (ch == KEY_MOUSE) {
                MEVENT event;
                if (getmouse(&event) == OK) {
                    sys->mouse_x = event.x;
                    sys->mouse_y = event.y;
                    if (event.bstate & BUTTON1_PRESSED) sys->mouse_buttons |= 1;
                    if (event.bstate & BUTTON1_RELEASED) sys->mouse_buttons &= ~1;
                    if (event.bstate & BUTTON3_PRESSED) sys->mouse_buttons |= 2;
                    if (event.bstate & BUTTON3_RELEASED) sys->mouse_buttons &= ~2;
#ifdef BUTTON4_PRESSED
                    if (event.bstate & BUTTON4_PRESSED) sys->mouse_wheel += 1;
                    if (event.bstate & BUTTON5_PRESSED) sys->mouse_wheel -= 1;
#endif
                }
            }
            ch = getch();
        }

        if (f_upd) m3_CallV(f_upd);

        int redraw = 0;
        for (int i = 0; i < 4; i++) {
            if (sys->signals[i] == 1) redraw = 1;
            else if (sys->signals[i] == 2) running = 0;
            sys->signals[i] = 0;
        }
        sys->mouse_wheel = 0;

        if (redraw) {
            int term_h, term_w;
            getmaxyx(stdscr, term_h, term_w);
            
            uint8_t* vram = mem + 512;
            
            // Simple scaling/clipping
            // If BPP != 8, we skip as per "hardcore" requirement
            if (BPP == 8) {
                for (int y = 0; y < term_h && y < (int)H; y++) {
                    for (int x = 0; x < term_w && x < (int)W; x++) {
                        uint8_t pixel = vram[y * W + x];
                        
                        // R3 G3 B2 to brightness
                        uint8_t r = (pixel >> 5) & 0x07;
                        uint8_t g = (pixel >> 2) & 0x07;
                        uint8_t b = (pixel & 0x03);
                        
                        // Simple weighted average for brightness
                        float norm_r = r / 7.0f;
                        float norm_g = g / 7.0f;
                        float norm_b = b / 3.0f;
                        float bright = (norm_r * 0.299f + norm_g * 0.587f + norm_b * 0.114f);
                        
                        int ramp_idx = (int)(bright * (RAMP_SIZE - 1));
                        if (ramp_idx < 0) ramp_idx = 0;
                        if (ramp_idx >= RAMP_SIZE) ramp_idx = RAMP_SIZE - 1;
                        
                        mvaddch(y, x, ASCII_RAMP[ramp_idx]);
                    }
                }
            } else {
                mvprintw(0, 0, "Unsupported BPP: %d (Only 8bpp supported)", BPP);
            }
            refresh();
        }

        napms(16); // ~60 FPS attempt
    }

    printf("\033[?1003l\n"); // Disable mouse reporting
    endwin();
    free(wasm_data);
    return 0;
}
