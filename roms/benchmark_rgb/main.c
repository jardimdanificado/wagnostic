typedef struct { int x, y, w, h; } Rect;
#include <stdint.h>

#define WASM_EXPORT __attribute__((visibility("default")))

typedef struct {
    uint32_t width, height, scale;
    char title[128];
    uint32_t dirty_rects;
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint8_t keys[256];
    uint32_t gamepad_buttons;
    uint32_t ticks;
    uint32_t target_fps;
    uint32_t audio_size, audio_sample_rate, audio_bpp, audio_channels;
    uint32_t audio_write, audio_read;
    uint32_t audio_underrun, audio_overrun;
    uint32_t audio_chunk_samples, audio_volume, audio_paused;
    uint32_t vram_offset;
    uint32_t audio_buffer_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    int32_t unique;
    uint8_t reserved[500];
} WagnosticState;

static struct { uint32_t count; Rect rects[32]; } my_dirty_list;

#define WIDTH  1280
#define HEIGHT 720

static struct {
    WagnosticState s;
    uint64_t vram[WIDTH * HEIGHT];
} rom;

static int initialized = 0;
static uint32_t frame_count = 0;
static uint32_t last_ticks = 0;
static uint32_t current_fps = 0;
static uint32_t current_frame_time_ms = 0;
static int current_mode = 0; // 0: RGB565 (16b Fast), 1: R5G1B5A5 (16b Slow), 2: RGBA8888 (32b Fast), 3: R11G11B10A10 (64b Exotico Slow)
static uint32_t mode_frames = 0;
static uint8_t prev_space = 0;

// Compact 8x8 font bitmap for ASCII 32..126
static const uint8_t font8x8[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 ' '
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // 33 !
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // 34 "
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // 35 #
    {0x0C,0x3E,0x03,0x1E,0x30,0x7C,0x18,0x00}, // 36 $
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // 37 %
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // 38 &
    {0x06,0x06,0x0C,0x00,0x00,0x00,0x00,0x00}, // 39 '
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // 40 (
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // 41 )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // 42 *
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // 43 +
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // 44 ,
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // 45 -
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // 46 .
    {0x00,0x03,0x06,0x0C,0x18,0x30,0x60,0x00}, // 47 /
    {0x3E,0x63,0x67,0x6F,0x7B,0x73,0x3E,0x00}, // 48 0
    {0x0C,0x1C,0x0C,0x0C,0x0C,0x0C,0x3E,0x00}, // 49 1
    {0x3E,0x63,0x06,0x1C,0x30,0x60,0x7F,0x00}, // 50 2
    {0x3E,0x63,0x06,0x1C,0x06,0x63,0x3E,0x00}, // 51 3
    {0x06,0x0E,0x1E,0x36,0x66,0x7F,0x06,0x00}, // 52 4
    {0x7F,0x60,0x7C,0x06,0x06,0x63,0x3E,0x00}, // 53 5
    {0x1C,0x30,0x60,0x7C,0x63,0x63,0x3E,0x00}, // 54 6
    {0x7F,0x63,0x06,0x0C,0x18,0x18,0x18,0x00}, // 55 7
    {0x3E,0x63,0x63,0x3E,0x63,0x63,0x3E,0x00}, // 56 8
    {0x3E,0x63,0x63,0x3E,0x06,0x0C,0x38,0x00}, // 57 9
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, // 58 :
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, // 59 ;
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // 60 <
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // 61 =
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // 62 >
    {0x3E,0x63,0x06,0x0C,0x18,0x00,0x18,0x00}, // 63 ?
    {0x3E,0x63,0x6F,0x6B,0x6F,0x60,0x3E,0x00}, // 64 @
    {0x1C,0x36,0x63,0x63,0x7F,0x63,0x63,0x00}, // 65 A
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // 66 B
    {0x1E,0x33,0x60,0x60,0x60,0x33,0x1E,0x00}, // 67 C
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // 68 D
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00}, // 69 E
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00}, // 70 F
    {0x1E,0x33,0x60,0x6E,0x63,0x33,0x1D,0x00}, // 71 G
    {0x63,0x63,0x63,0x7F,0x63,0x63,0x63,0x00}, // 72 H
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 73 I
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, // 74 J
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // 75 K
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // 76 L
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // 77 M
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // 78 N
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // 79 O
    {0x7C,0x63,0x63,0x7C,0x60,0x60,0x60,0x00}, // 80 P
    {0x1C,0x36,0x63,0x63,0x6B,0x36,0x1D,0x00}, // 81 Q
    {0x7C,0x63,0x63,0x7C,0x6C,0x66,0x63,0x00}, // 82 R
    {0x3E,0x63,0x60,0x3E,0x03,0x63,0x3E,0x00}, // 83 S
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 84 T
    {0x63,0x63,0x63,0x63,0x63,0x63,0x3E,0x00}, // 85 U
    {0x63,0x63,0x63,0x63,0x36,0x1C,0x08,0x00}, // 86 V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 87 W
    {0x63,0x63,0x36,0x1C,0x36,0x63,0x63,0x00}, // 88 X
    {0x63,0x63,0x36,0x1C,0x18,0x18,0x18,0x00}, // 89 Y
    {0x7F,0x03,0x06,0x1C,0x30,0x60,0x7F,0x00}, // 90 Z
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // 91 [
    {0x00,0x60,0x30,0x18,0x0C,0x06,0x03,0x00}, // 92 \
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // 93 ]
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // 94 ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // 95 _
    {0x18,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // 96 `
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3B,0x00}, // 97 a
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // 98 b
    {0x00,0x00,0x3C,0x60,0x60,0x60,0x3C,0x00}, // 99 c
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, // 100 d
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, // 101 e
    {0x1C,0x30,0x7C,0x30,0x30,0x30,0x30,0x00}, // 102 f
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C}, // 103 g
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, // 104 h
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // 105 i
    {0x06,0x00,0x06,0x06,0x06,0x66,0x3C,0x00}, // 106 j
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, // 107 k
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 108 l
    {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, // 109 m
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, // 110 n
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // 111 o
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, // 112 p
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, // 113 q
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, // 114 r
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, // 115 s
    {0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00}, // 116 t
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3B,0x00}, // 117 u
    {0x00,0x00,0x63,0x63,0x36,0x1C,0x08,0x00}, // 118 v
    {0x00,0x00,0x63,0x63,0x6B,0x7F,0x36,0x00}, // 119 w
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // 120 x
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}, // 121 y
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, // 122 z
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // 123 {
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // 124 |
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // 125 }
    {0x3B,0x6E,0x00,0x00,0x00,0x00,0x00,0x00}  // 126 ~
};

static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    int idx = y * WIDTH + x;
    if (current_mode == 0) {
        // Mode 0: RGB565 (16-bit Fast Path)
        uint16_t *v16 = (uint16_t*)rom.vram;
        v16[idx] = ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3);
    } else if (current_mode == 1) {
        // Mode 1: R5G1B5A5 (16-bit Exotic / Fallback)
        uint16_t *v16 = (uint16_t*)rom.vram;
        v16[idx] = ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 7) << 10) | ((uint16_t)(b >> 3) << 5) | (a >> 3);
    } else if (current_mode == 2) {
        // Mode 2: RGBA8888 (32-bit Fast Path)
        uint32_t *v32 = (uint32_t*)rom.vram;
        v32[idx] = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    } else if (current_mode == 3) {
        // Mode 3: R11G11B10A10 (64-bit Exotic / Fallback)
        uint64_t *v64 = rom.vram;
        uint64_t r11 = (uint64_t)r * 2047 / 255;
        uint64_t g11 = (uint64_t)g * 2047 / 255;
        uint64_t b10 = (uint64_t)b * 1023 / 255;
        uint64_t a10 = (uint64_t)a * 1023 / 255;
        v64[idx] = (a10 << 32) | (b10 << 22) | (g11 << 11) | r11;
    }
}

static void draw_fill_rect(int rx, int ry, int rw, int rh, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int y = ry; y < ry + rh; y++) {
        for (int x = rx; x < rx + rw; x++) {
            set_pixel(x, y, r, g, b, a);
        }
    }
}

static void draw_char(int x, int y, char c, uint8_t r, uint8_t g, uint8_t b) {
    if (c < 32 || c > 126) c = '?';
    int c_idx = c - 32;
    for (int row = 0; row < 8; row++) {
        uint8_t bits = font8x8[c_idx][row];
        for (int col = 0; col < 8; col++) {
            if ((bits >> (7 - col)) & 1) {
                set_pixel(x + col, y + row, r, g, b, 255);
            }
        }
    }
}

static void draw_string(int x, int y, const char* str, uint8_t r, uint8_t g, uint8_t b) {
    int cx = x;
    while (*str) {
        draw_char(cx, y, *str, r, g, b);
        cx += 8;
        str++;
    }
}

static void apply_mode_settings() {
    if (current_mode == 0) {
        // Mode 0: RGB565 (16-bit Otimizado / Fast Path)
        rom.s.r_bits = 5; rom.s.r_shift = 11;
        rom.s.g_bits = 6; rom.s.g_shift = 5;
        rom.s.b_bits = 5; rom.s.b_shift = 0;
        rom.s.a_bits = 0; rom.s.a_shift = 0;
        rom.s.x_bits = 0; rom.s.x_shift = 0;
    } else if (current_mode == 1) {
        // Mode 1: R5G1B5A5 (16-bit Nao-Padronizado / Generic Slow Fallback)
        rom.s.r_bits = 5; rom.s.r_shift = 11;
        rom.s.g_bits = 1; rom.s.g_shift = 10;
        rom.s.b_bits = 5; rom.s.b_shift = 5;
        rom.s.a_bits = 5; rom.s.a_shift = 0;
        rom.s.x_bits = 0; rom.s.x_shift = 0;
    } else if (current_mode == 2) {
        // Mode 2: RGBA8888 (32-bit Otimizado / Fast Path)
        rom.s.r_bits = 8; rom.s.r_shift = 0;
        rom.s.g_bits = 8; rom.s.g_shift = 8;
        rom.s.b_bits = 8; rom.s.b_shift = 16;
        rom.s.a_bits = 8; rom.s.a_shift = 24;
        rom.s.x_bits = 0; rom.s.x_shift = 0;
    } else if (current_mode == 3) {
        // Mode 3: R11G11B10A10 (64-bit Exotico Nao-Padronizado / Generic Slow Fallback)
        rom.s.r_bits = 11; rom.s.r_shift = 0;
        rom.s.g_bits = 11; rom.s.g_shift = 11;
        rom.s.b_bits = 10; rom.s.b_shift = 22;
        rom.s.a_bits = 10; rom.s.a_shift = 32;
        rom.s.x_bits = 0;  rom.s.x_shift = 0;
    }
}

static void int_to_str(uint32_t val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16];
    int len = 0;
    while (val > 0) {
        tmp[len++] = '0' + (val % 10);
        val /= 10;
    }
    for (int i = 0; i < len; i++) {
        buf[i] = tmp[len - 1 - i];
    }
    buf[len] = '\0';
}

static void fill_vram_fast(uint32_t t) {
    int total = WIDTH * HEIGHT;
    uint32_t frame = t / 16;
    if (current_mode == 0) {
        // Mode 0: 16-bit RGB565 (Fast Path) - Bright Cyan/Purple pattern
        uint16_t *v16 = (uint16_t*)rom.vram;
        uint16_t c1 = (uint16_t)((28 << 11) | (((frame) & 0x3F) << 5) | 31);
        uint16_t c2 = (uint16_t)((((frame) & 0x1F) << 11) | (55 << 5) | 15);
        for (int i = 0; i < total; i++) v16[i] = ((i ^ (i >> 6)) & 1) ? c1 : c2;
    } else if (current_mode == 1) {
        // Mode 1: 16-bit R5G1B5A5 (Exotic Slow Fallback) - Bright Red/Blue pattern
        uint16_t *v16 = (uint16_t*)rom.vram;
        uint16_t c1 = (uint16_t)((28 << 11) | (1 << 10) | (((frame) & 0x1F) << 5) | 31);
        uint16_t c2 = (uint16_t)((((frame) & 0x1F) << 11) | (1 << 10) | (28 << 5) | 31);
        for (int i = 0; i < total; i++) v16[i] = ((i ^ (i >> 6)) & 1) ? c1 : c2;
    } else if (current_mode == 2) {
        // Mode 2: 32-bit RGBA8888 (Fast Path) - Bright Gold/Blue pattern
        uint32_t *v32 = (uint32_t*)rom.vram;
        uint32_t c1 = 0xFF00E0FF | (((frame * 4) & 0xFF) << 8);
        uint32_t c2 = 0xFFFF8000 | ((frame * 2) & 0xFF);
        for (int i = 0; i < total; i++) v32[i] = ((i ^ (i >> 6)) & 1) ? c1 : c2;
    } else if (current_mode == 3) {
        // Mode 3: 64-bit R11G11B10A10 (Exotic >32b Slow Fallback) - Bright Magenta/Green pattern
        uint64_t *v64 = rom.vram;
        uint64_t r1 = 1800, g1 = (frame * 10) & 0x7FF, b1 = 800, a1 = 1023;
        uint64_t r2 = (frame * 8) & 0x7FF, g2 = 1900, b2 = 900, a2 = 1023;
        uint64_t c1 = (a1 << 32) | (b1 << 22) | (g1 << 11) | r1;
        uint64_t c2 = (a2 << 32) | (b2 << 22) | (g2 << 11) | r2;
        for (int i = 0; i < total; i++) v64[i] = ((i ^ (i >> 6)) & 1) ? c1 : c2;
    }
}

WASM_EXPORT int wupdate() {
    if (!initialized) {
        rom.s.width = WIDTH;
        rom.s.height = HEIGHT;
        rom.s.scale = 1;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        apply_mode_settings();
        
        const char* t_str = "Wagnostic RGB High-Load Benchmark";
        int i = 0;
        while (t_str[i] && i < 127) { rom.s.title[i] = t_str[i]; i++; }
        rom.s.title[i] = '\0';
        
        initialized = 1;
    }

    frame_count++;
    mode_frames++;

    // Switch mode every 250 frames (~4 seconds at 60 FPS) or on spacebar/click
    uint8_t space_down = rom.s.keys[0x2C] || (rom.s.mouse_buttons & 1);
    if ((space_down && !prev_space) || mode_frames >= 250) {
        current_mode = (current_mode + 1) % 4;
        apply_mode_settings();
        mode_frames = 0;
    }
    prev_space = space_down;

    // Calculate FPS and frame time
    if (rom.s.ticks - last_ticks >= 1000) {
        current_fps = frame_count;
        current_frame_time_ms = (frame_count > 0) ? (1000 / frame_count) : 0;
        frame_count = 0;
        last_ticks = rom.s.ticks;
    }

    uint32_t t = rom.s.ticks;
    
    // 1. Ultra-fast WASM VRAM fill (pushing 100% host unpack load)
    fill_vram_fast(t);

    // 2. Draw HUD / UI Overlay box (720x130 pixels)
    draw_fill_rect(20, 20, 720, 130, 0, 0, 0, 255);

    // Title & Status
    draw_string(30, 30, "=== WAGNOSTIC RGB HEAVY BENCHMARK (1280x720) ===", 255, 255, 0);

    if (current_mode == 0) {
        draw_string(30, 50, "MODE 0: [16-BIT FAST-PATH] RGB565 (R5 G6 B5)", 0, 255, 128);
        draw_string(30, 65, "STATUS: DIRECT OPTIMIZED DECODER ACTIVE (FAST SHIFTS)", 0, 255, 0);
    } else if (current_mode == 1) {
        draw_string(30, 50, "MODE 1: [16-BIT UNSTANDARDIZED SLOW] R5G1B5A5 (EXOTIC)", 255, 100, 100);
        draw_string(30, 65, "STATUS: GENERIC SLOW FALLBACK ACTIVE (DYNAMIC MATH PER PX)", 255, 50, 50);
    } else if (current_mode == 2) {
        draw_string(30, 50, "MODE 2: [32-BIT FAST-PATH] RGBA8888 (R8 G8 B8 A8)", 0, 255, 128);
        draw_string(30, 65, "STATUS: DIRECT OPTIMIZED DECODER ACTIVE (DIRECT MEMCPY)", 0, 255, 0);
    } else if (current_mode == 3) {
        draw_string(30, 50, "MODE 3: [64-BIT UNSTANDARDIZED SLOW] R11G11B10A10 (>32b EXOTIC)", 255, 80, 255);
        draw_string(30, 65, "STATUS: GENERIC SLOW FALLBACK ACTIVE (64-BIT DYNAMIC MATH)", 255, 50, 50);
    }

    // FPS & Frame time
    char fps_str[16], ft_str[16];
    int_to_str(current_fps, fps_str);
    int_to_str(current_frame_time_ms, ft_str);

    draw_string(30, 90, "FPS: ", 255, 255, 255);
    draw_string(70, 90, fps_str, 255, 255, 0);
    draw_string(140, 90, " | FRAME TIME: ", 255, 255, 255);
    draw_string(260, 90, ft_str, 255, 255, 0);
    draw_string(295, 90, " ms", 255, 255, 255);

    draw_string(30, 115, "CONTROLS: PRESS SPACE OR CLICK MOUSE TO SWITCH MODE", 200, 200, 200);

    my_dirty_list.count = 1;
    my_dirty_list.rects[0] = (Rect){0, 0, WIDTH, HEIGHT};
    rom.s.dirty_rects = (uint32_t)&my_dirty_list;

    return (int)&rom.s;
}
