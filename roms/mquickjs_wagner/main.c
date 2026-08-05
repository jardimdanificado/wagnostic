#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* --- Assets & Wagner defines --- */
#include "assets.h"
typedef WagnosticAsset WagnerAsset;
#define WAGNER_ASSETS WAGNOSTIC_ASSETS
#define WAGNER_ASSET_COUNT WAGNOSTIC_ASSET_COUNT
#define WAGNER_TITLE "mquickjs_wagner"

/* --- Wagner Framework --- */
#include "wagner.h"

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
JSValue js_set_pixel(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { return JS_UNDEFINED; }

/* Wagner JS C-Function Declarations */
JSValue js_push(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_pop(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_translate(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_rotate(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_scale(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_fill(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_no_fill(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_stroke(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_no_stroke(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_clear(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_rect(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_circle(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_triangle(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_triangle_pts(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_line(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_pixel(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_text(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_rgb(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_rgba(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_play_tone(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_play_noise(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_stop_all_sounds(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

#include "mqjs_stdlib.h"

static uint8_t js_heap[512 * 1024];
static JSContext *ctx = NULL;
static JSValue js_wagnostic_obj = JS_UNDEFINED;

static void report_exception(JSContext *ctx, JSValue res) {
    if (JS_IsException(res)) {
        JSValue exc = JS_GetException(ctx);
        JSCStringBuf buf;
        const char *str = JS_ToCString(ctx, exc, &buf);
        if (str) {
            strncpy(_wagner_rom.state.title, str, 127);
            _wagner_rom.state.title[127] = '\0';
        }
    }
}

/* Function Implementations */
JSValue js_push(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    push();
    return JS_UNDEFINED;
}

JSValue js_pop(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    pop();
    return JS_UNDEFINED;
}

JSValue js_translate(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    double x = 0, y = 0;
    if (argc > 0) JS_ToNumber(ctx, &x, argv[0]);
    if (argc > 1) JS_ToNumber(ctx, &y, argv[1]);
    translate((float)x, (float)y);
    return JS_UNDEFINED;
}

JSValue js_rotate(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    double angle = 0;
    if (argc > 0) JS_ToNumber(ctx, &angle, argv[0]);
    rotate((float)angle);
    return JS_UNDEFINED;
}

JSValue js_scale(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    double sx = 1.0, sy = 1.0;
    if (argc > 0) JS_ToNumber(ctx, &sx, argv[0]);
    if (argc > 1) JS_ToNumber(ctx, &sy, argv[1]);
    else sy = sx;
    scale((float)sx, (float)sy);
    return JS_UNDEFINED;
}

JSValue js_fill(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc == 1) {
        int col = 0;
        JS_ToInt32(ctx, &col, argv[0]);
        fill((pixel_t)col);
    } else if (argc == 3) {
        int r = 0, g = 0, b = 0;
        JS_ToInt32(ctx, &r, argv[0]);
        JS_ToInt32(ctx, &g, argv[1]);
        JS_ToInt32(ctx, &b, argv[2]);
        fill(rgb(r, g, b));
    } else if (argc >= 4) {
        int r = 0, g = 0, b = 0, a = 255;
        JS_ToInt32(ctx, &r, argv[0]);
        JS_ToInt32(ctx, &g, argv[1]);
        JS_ToInt32(ctx, &b, argv[2]);
        JS_ToInt32(ctx, &a, argv[3]);
        fill(rgba(r, g, b, a));
    }
    return JS_UNDEFINED;
}

JSValue js_no_fill(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    no_fill();
    return JS_UNDEFINED;
}

JSValue js_stroke(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc == 1) {
        int col = 0;
        JS_ToInt32(ctx, &col, argv[0]);
        stroke((pixel_t)col);
    } else if (argc == 3) {
        int r = 0, g = 0, b = 0;
        JS_ToInt32(ctx, &r, argv[0]);
        JS_ToInt32(ctx, &g, argv[1]);
        JS_ToInt32(ctx, &b, argv[2]);
        stroke(rgb(r, g, b));
    } else if (argc >= 4) {
        int r = 0, g = 0, b = 0, a = 255;
        JS_ToInt32(ctx, &r, argv[0]);
        JS_ToInt32(ctx, &g, argv[1]);
        JS_ToInt32(ctx, &b, argv[2]);
        JS_ToInt32(ctx, &a, argv[3]);
        stroke(rgba(r, g, b, a));
    }
    return JS_UNDEFINED;
}

JSValue js_no_stroke(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    no_stroke();
    return JS_UNDEFINED;
}

JSValue js_clear(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    clear();
    return JS_UNDEFINED;
}

JSValue js_rect(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 4) {
        double x = 0, y = 0, w = 0, h = 0;
        JS_ToNumber(ctx, &x, argv[0]);
        JS_ToNumber(ctx, &y, argv[1]);
        JS_ToNumber(ctx, &w, argv[2]);
        JS_ToNumber(ctx, &h, argv[3]);
        push();
        translate((float)x, (float)y);
        scale((float)w, (float)h);
        rect();
        pop();
    } else {
        rect();
    }
    return JS_UNDEFINED;
}

JSValue js_circle(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 3) {
        double x = 0, y = 0, r = 0;
        JS_ToNumber(ctx, &x, argv[0]);
        JS_ToNumber(ctx, &y, argv[1]);
        JS_ToNumber(ctx, &r, argv[2]);
        push();
        translate((float)x, (float)y);
        scale((float)r, (float)r);
        circle();
        pop();
    } else {
        circle();
    }
    return JS_UNDEFINED;
}

JSValue js_triangle(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 6) {
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
        JS_ToNumber(ctx, &x1, argv[0]); JS_ToNumber(ctx, &y1, argv[1]);
        JS_ToNumber(ctx, &x2, argv[2]); JS_ToNumber(ctx, &y2, argv[3]);
        JS_ToNumber(ctx, &x3, argv[4]); JS_ToNumber(ctx, &y3, argv[5]);
        triangle_pts((float)x1, (float)y1, (float)x2, (float)y2, (float)x3, (float)y3);
    } else {
        triangle();
    }
    return JS_UNDEFINED;
}

JSValue js_triangle_pts(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 6) {
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
        JS_ToNumber(ctx, &x1, argv[0]); JS_ToNumber(ctx, &y1, argv[1]);
        JS_ToNumber(ctx, &x2, argv[2]); JS_ToNumber(ctx, &y2, argv[3]);
        JS_ToNumber(ctx, &x3, argv[4]); JS_ToNumber(ctx, &y3, argv[5]);
        triangle_pts((float)x1, (float)y1, (float)x2, (float)y2, (float)x3, (float)y3);
    }
    return JS_UNDEFINED;
}

JSValue js_line(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 4) {
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        JS_ToNumber(ctx, &x1, argv[0]); JS_ToNumber(ctx, &y1, argv[1]);
        JS_ToNumber(ctx, &x2, argv[2]); JS_ToNumber(ctx, &y2, argv[3]);
        line((float)x1, (float)y1, (float)x2, (float)y2);
    }
    return JS_UNDEFINED;
}

JSValue js_pixel(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 2) {
        double x = 0, y = 0;
        JS_ToNumber(ctx, &x, argv[0]);
        JS_ToNumber(ctx, &y, argv[1]);
        pixel((float)x, (float)y);
    }
    return JS_UNDEFINED;
}

JSValue js_text(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 1) {
        JSCStringBuf sbuf;
        const char *str = JS_ToCString(ctx, argv[0], &sbuf);
        if (str) {
            if (argc >= 3) {
                double x = 0, y = 0;
                JS_ToNumber(ctx, &x, argv[1]);
                JS_ToNumber(ctx, &y, argv[2]);
                push();
                translate((float)x, (float)y);
                text(str);
                pop();
            } else {
                text(str);
            }
        }
    }
    return JS_UNDEFINED;
}

JSValue js_rgb(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    int r = 0, g = 0, b = 0;
    if (argc > 0) JS_ToInt32(ctx, &r, argv[0]);
    if (argc > 1) JS_ToInt32(ctx, &g, argv[1]);
    if (argc > 2) JS_ToInt32(ctx, &b, argv[2]);
    return JS_NewInt32(ctx, (int32_t)rgb(r, g, b));
}

JSValue js_rgba(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    int r = 0, g = 0, b = 0, a = 255;
    if (argc > 0) JS_ToInt32(ctx, &r, argv[0]);
    if (argc > 1) JS_ToInt32(ctx, &g, argv[1]);
    if (argc > 2) JS_ToInt32(ctx, &b, argv[2]);
    if (argc > 3) JS_ToInt32(ctx, &a, argv[3]);
    return JS_NewInt32(ctx, (int32_t)rgba(r, g, b, a));
}

JSValue js_play_tone(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    double freq = 440.0, dur = 0.1, vol = 0.5;
    if (argc > 0) JS_ToNumber(ctx, &freq, argv[0]);
    if (argc > 1) JS_ToNumber(ctx, &dur, argv[1]);
    if (argc > 2) JS_ToNumber(ctx, &vol, argv[2]);
    play_tone((float)freq, (float)dur, (float)vol);
    return JS_UNDEFINED;
}

JSValue js_play_noise(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    double dur = 0.1, vol = 0.5;
    if (argc > 0) JS_ToNumber(ctx, &dur, argv[0]);
    if (argc > 1) JS_ToNumber(ctx, &vol, argv[1]);
    play_noise((float)dur, (float)vol);
    return JS_UNDEFINED;
}

JSValue js_stop_all_sounds(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    stop_all_sounds();
    return JS_UNDEFINED;
}

/* Wagner Setup & Draw callbacks (invoked by wagner.h) */

void setup() {
    ctx = JS_NewContext(js_heap, sizeof(js_heap), &js_stdlib);
    if (!ctx) {
        strcpy(_wagner_rom.state.title, "JS_NewContext failed");
        return;
    }
    
    JSValue global = JS_GetGlobalObject(ctx);
    
    // Wagner state object
    js_wagnostic_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "width", JS_NewInt32(ctx, wagner.width));
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "height", JS_NewInt32(ctx, wagner.height));
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "mouse_x", JS_NewInt32(ctx, (int)wagner.mouse.x));
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "mouse_y", JS_NewInt32(ctx, (int)wagner.mouse.y));
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "mouse_down", JS_NewBool(wagner.mouse_down));
    JS_SetPropertyStr(ctx, global, "wagnostic", js_wagnostic_obj);
    JS_SetPropertyStr(ctx, global, "wagner", js_wagnostic_obj);

    // Color constants
    JS_SetPropertyStr(ctx, global, "BLACK", JS_NewInt32(ctx, (int32_t)BLACK));
    JS_SetPropertyStr(ctx, global, "WHITE", JS_NewInt32(ctx, (int32_t)WHITE));
    JS_SetPropertyStr(ctx, global, "RED", JS_NewInt32(ctx, (int32_t)RED));
    JS_SetPropertyStr(ctx, global, "GREEN", JS_NewInt32(ctx, (int32_t)GREEN));
    JS_SetPropertyStr(ctx, global, "BLUE", JS_NewInt32(ctx, (int32_t)BLUE));
    JS_SetPropertyStr(ctx, global, "YELLOW", JS_NewInt32(ctx, (int32_t)YELLOW));
    JS_SetPropertyStr(ctx, global, "CYAN", JS_NewInt32(ctx, (int32_t)CYAN));
    JS_SetPropertyStr(ctx, global, "MAGENTA", JS_NewInt32(ctx, (int32_t)MAGENTA));
    JS_SetPropertyStr(ctx, global, "GRAY", JS_NewInt32(ctx, (int32_t)GRAY));
    JS_SetPropertyStr(ctx, global, "ORANGE", JS_NewInt32(ctx, (int32_t)ORANGE));
    JS_SetPropertyStr(ctx, global, "PURPLE", JS_NewInt32(ctx, (int32_t)PURPLE));

    // Key constants
    JS_SetPropertyStr(ctx, global, "KEY_UP", JS_NewInt32(ctx, KEY_UP));
    JS_SetPropertyStr(ctx, global, "KEY_DOWN", JS_NewInt32(ctx, KEY_DOWN));
    JS_SetPropertyStr(ctx, global, "KEY_LEFT", JS_NewInt32(ctx, KEY_LEFT));
    JS_SetPropertyStr(ctx, global, "KEY_RIGHT", JS_NewInt32(ctx, KEY_RIGHT));
    JS_SetPropertyStr(ctx, global, "KEY_SPACE", JS_NewInt32(ctx, KEY_SPACE));
    JS_SetPropertyStr(ctx, global, "KEY_ENTER", JS_NewInt32(ctx, KEY_ENTER));

    // Load embedded game.js asset
    const char *script_code = NULL;
    size_t script_len = 0;
    for (int i = 0; i < WAGNOSTIC_ASSET_COUNT; i++) {
        if (strcmp(WAGNOSTIC_ASSETS[i].path, "game.js") == 0) {
            script_code = (const char*)WAGNOSTIC_ASSETS[i].data;
            script_len = WAGNOSTIC_ASSETS[i].size;
            break;
        }
    }

    if (script_code) {
        JSValue res = JS_Eval(ctx, script_code, script_len, "game.js", 0);
        report_exception(ctx, res);
    } else {
        strcpy(_wagner_rom.state.title, "game.js not found in assets");
    }

    // Call JS setup() if available
    JSValue js_setup_func = JS_GetPropertyStr(ctx, global, "setup");
    if (JS_IsFunction(ctx, js_setup_func)) {
        if (!JS_StackCheck(ctx, 2)) {
            JS_PushArg(ctx, js_setup_func);
            JS_PushArg(ctx, JS_NULL);
            JSValue res = JS_Call(ctx, 0);
            report_exception(ctx, res);
        }
    }
}

void draw() {
    if (!ctx) {
        strcpy(_wagner_rom.state.title, "ctx is NULL");
        return;
    }
    
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue wag_obj = JS_GetPropertyStr(ctx, global, "wagner");
    
    // Update mouse and state properties
    JS_SetPropertyStr(ctx, wag_obj, "mouse_x", JS_NewInt32(ctx, (int)wagner.mouse.x));
    JS_SetPropertyStr(ctx, wag_obj, "mouse_y", JS_NewInt32(ctx, (int)wagner.mouse.y));
    JS_SetPropertyStr(ctx, wag_obj, "mouse_down", JS_NewBool(wagner.mouse_down));
    JS_SetPropertyStr(ctx, wag_obj, "fps", JS_NewInt32(ctx, wagner.fps));
    JS_SetPropertyStr(ctx, wag_obj, "frame_count", JS_NewInt32(ctx, wagner.frame_count));
    
    // Call JS draw() function
    JSValue js_draw_func = JS_GetPropertyStr(ctx, global, "draw");
    if (JS_IsFunction(ctx, js_draw_func)) {
        if (!JS_StackCheck(ctx, 2)) {
            JS_PushArg(ctx, js_draw_func);
            JS_PushArg(ctx, JS_NULL);
            JSValue res = JS_Call(ctx, 0);
            report_exception(ctx, res);
        }
    } else {
        strcpy(_wagner_rom.state.title, "draw function not found");
    }
}
