#include <stdint.h>
#include <stddef.h>

// Import Wagnostic extension dispatcher
extern void* wextension(const char* name, void* ptr);

typedef struct { int x, y, w, h; } Rect;

typedef struct {
    uint32_t width, height, scale;
    uint32_t dirty_rects;
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint8_t keys[256];
    uint32_t gamepad_buttons;
    uint32_t ticks;
    uint32_t target_fps;
    uint32_t vram_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    int32_t unique;
    uint8_t reserved[676];
} State;

static struct { uint32_t count; Rect rects[32]; } my_dirty_list;

static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
} rom;

static uint16_t* fb = (uint16_t*)rom.vram;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static int initialized = 0;
static int test_passed = 0;
static char get_buf[128];

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        rom.s.r_bits = 5; rom.s.r_shift = 11;
        rom.s.g_bits = 6; rom.s.g_shift = 5;
        rom.s.b_bits = 5; rom.s.b_shift = 0;
        
        // Test 1: Unknown extension returns NULL
        void* res1 = wextension("unknown:custom_feature", NULL);
        
        // Test 2: Set window title via title.set extension
        const char* my_title = "Wagnostic Title Extension Test";
        void* res2 = wextension("title.set", (void*)my_title);

        // Test 3: Get window title via title.get extension
        get_buf[0] = '\0';
        void* res3 = wextension("title.get", get_buf);

        test_passed = (res1 == NULL) && (res2 != NULL) && (res3 != NULL) && (strcmp(get_buf, my_title) == 0);

        initialized = 1;
    }

    uint16_t color = test_passed ? rgb565(30, 180, 50) : rgb565(200, 30, 30);
    for (int i = 0; i < 320 * 240; i++) {
        fb[i] = color;
    }

    my_dirty_list.count = 1;
    my_dirty_list.rects[0] = (Rect){0, 0, 320, 240};
    rom.s.dirty_rects = (uint32_t)&my_dirty_list;

    return (int)&rom.s;
}
