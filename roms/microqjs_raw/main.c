#include <stdint.h>
#include <string.h>
#include <stdlib.h>

extern void* wextension(const char* name, void* ptr);

/* --- Wagnostic ABI --- */
typedef struct { int x, y, w, h; } Rect;

typedef struct {
    uint32_t width, height;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t vram_offset;
} WagnosticState;

typedef struct {
    int32_t x, y;
    uint32_t buttons;
    int32_t wheel;
} MouseState;

_Static_assert(sizeof(WagnosticState) == 44, "Size mismatch");

#define WIDTH 320
#define HEIGHT 240

static struct {
    WagnosticState s;
    uint8_t vram[WIDTH * HEIGHT * 4];
} rom = {0};

/* --- MicroQuickJS --- */
#include "mquickjs.h"

/* Stubs for JS stdlib optional functions */
JSValue js_date_constructor(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_NewDate(ctx, 0); }
JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_NewInt64(ctx, 0); }
JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_NewInt64(ctx, 0); }
JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }
JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { JS_GC(ctx); return JS_UNDEFINED; }
JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }
JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_NewInt32(ctx, 0); }
JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }
JSValue js_set_pixel(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

#include "mqjs_stdlib.h"

/* Assets */
#include "assets.h"

static uint8_t js_heap[512 * 1024];
static JSContext *ctx = NULL;
static JSValue js_frame_func = JS_UNDEFINED;
static JSValue js_wagnostic_obj = JS_UNDEFINED;
static MouseState mouse_buf;
static MouseState* mouse = NULL;
static uint32_t ticks = 0;

/* Native function: set_pixel(x, y, r, g, b) */
JSValue js_set_pixel(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc < 5) return JS_UNDEFINED;
    
    int x = 0, y = 0, r = 0, g = 0, b = 0;
    JS_ToInt32(ctx, &x, argv[0]);
    JS_ToInt32(ctx, &y, argv[1]);
    JS_ToInt32(ctx, &r, argv[2]);
    JS_ToInt32(ctx, &g, argv[3]);
    JS_ToInt32(ctx, &b, argv[4]);
    
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        uint32_t *vram32 = (uint32_t*)rom.vram;
        /* Assuming RGBA8888 Host format (A=24, B=16, G=8, R=0) */
        uint32_t color = (255 << 24) | ((b & 0xFF) << 16) | ((g & 0xFF) << 8) | (r & 0xFF);
        vram32[y * WIDTH + x] = color;
    }
    
    return JS_UNDEFINED;
}

/* Wagnostic ABI Entry */
int wupdate() {
    ticks++;
    // Frame 0 Initialization
    if (rom.s.width == 0) {
        rom.s.width = WIDTH;
        rom.s.height = HEIGHT;
        
        // Host format: RGBA8888
        rom.s.r_bits = 8; rom.s.r_shift = 0;
        rom.s.g_bits = 8; rom.s.g_shift = 8;
        rom.s.b_bits = 8; rom.s.b_shift = 16;
        rom.s.a_bits = 8; rom.s.a_shift = 24;
        
        rom.s.vram_offset = sizeof(WagnosticState);

        mouse = (MouseState*)wextension("std:mouse", &mouse_buf);

        // Init JS
        ctx = JS_NewContext(js_heap, sizeof(js_heap), &js_stdlib);
        if (ctx) {
            JSValue global = JS_GetGlobalObject(ctx);
            
            // Add wagnostic object
            js_wagnostic_obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, js_wagnostic_obj, "width", JS_NewInt32(ctx, WIDTH));
            JS_SetPropertyStr(ctx, js_wagnostic_obj, "height", JS_NewInt32(ctx, HEIGHT));
            JS_SetPropertyStr(ctx, js_wagnostic_obj, "mouse_x", JS_NewInt32(ctx, 0));
            JS_SetPropertyStr(ctx, js_wagnostic_obj, "mouse_y", JS_NewInt32(ctx, 0));
            JS_SetPropertyStr(ctx, global, "wagnostic", js_wagnostic_obj);
            
            // JavaScript code embedded directly as a string (Exact original demo)
            const char *script_code = 
                "var t = 0;\n"
                "function frame() {\n"
                "  var w = wagnostic.width;\n"
                "  var h = wagnostic.height;\n"
                "  t += 1;\n"
                "  for (var y = 0; y < h; y++) {\n"
                "    for (var x = 0; x < w; x++) {\n"
                "      var v = (x ^ y) + t;\n"
                "      set_pixel(x, y, v % 255, (v * 2) % 255, (v * 3) % 255);\n"
                "    }\n"
                "  }\n"
                "  var mx = wagnostic.mouse_x;\n"
                "  var my = wagnostic.mouse_y;\n"
                "  for (var dy = -5; dy <= 5; dy++) {\n"
                "    for (var dx = -5; dx <= 5; dx++) {\n"
                "      set_pixel(mx + dx, my + dy, 255, 0, 0);\n"
                "    }\n"
                "  }\n"
                "}\n";
            
            if (script_code) {
                JSValue res = JS_Eval(ctx, script_code, strlen(script_code), "main.js", 0);
                if (JS_IsException(res)) {
                    JS_GetException(ctx);
                }
            }
        }
    }
    
    if (ctx) {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue wagnostic_obj = JS_GetPropertyStr(ctx, global, "wagnostic");
        
        // Update input & ticks to JS
        JS_SetPropertyStr(ctx, wagnostic_obj, "mouse_x", JS_NewInt32(ctx, mouse ? mouse->x : 0));
        JS_SetPropertyStr(ctx, wagnostic_obj, "mouse_y", JS_NewInt32(ctx, mouse ? mouse->y : 0));
        JS_SetPropertyStr(ctx, wagnostic_obj, "ticks", JS_NewInt32(ctx, ticks));
        
        // Call frame()
        JSValue js_frame = JS_GetPropertyStr(ctx, global, "frame");
        if (JS_IsFunction(ctx, js_frame)) {
            if (!JS_StackCheck(ctx, 2)) {
                JS_PushArg(ctx, js_frame);
                JS_PushArg(ctx, JS_NULL);
                JSValue res = JS_Call(ctx, 0);
                if (JS_IsException(res)) {
                    // Clear exception state so runtime doesn't lock up
                    JS_GetException(ctx);
                }
            }
        }
    }
    
    return (int)&rom.s;
}
