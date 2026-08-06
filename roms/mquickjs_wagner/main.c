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
#define STB_IMAGE_IMPLEMENTATION
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
JSValue js_is_key_down(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_is_key_pressed(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_set_title(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_set_size(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_set_scale(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_load_image(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_create_image(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_texture(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_no_texture(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_color_key(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_no_color_key(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_image(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_load_gif_anim(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

#include "mqjs_stdlib.h"

static uint8_t js_heap[512 * 1024];
static JSContext *ctx = NULL;
static JSValue js_wagnostic_obj = JS_UNDEFINED;

#define MAX_JS_IMAGES 64
static Image js_images[MAX_JS_IMAGES];
static int js_image_count = 0;

static void report_exception(JSContext *ctx, JSValue res) {
    if (JS_IsException(res)) {
        JSValue exc = JS_GetException(ctx);
        JSCStringBuf buf;
        const char *str = JS_ToCString(ctx, exc, &buf);
        (void)str;
    }
}

static JSValue create_js_image_obj(JSContext *ctx, int id, int w, int h) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "_id", JS_NewInt32(ctx, id));
    JS_SetPropertyStr(ctx, obj, "width", JS_NewInt32(ctx, w));
    JS_SetPropertyStr(ctx, obj, "height", JS_NewInt32(ctx, h));
    return obj;
}

static int get_js_image_id(JSContext *ctx, JSValue val) {
    if (JS_IsInt(val)) {
        int id = -1;
        JS_ToInt32(ctx, &id, val);
        return id;
    }
    if (JS_IsPtr(val)) {
        JSValue id_val = JS_GetPropertyStr(ctx, val, "_id");
        if (JS_IsInt(id_val)) {
            int id = -1;
            JS_ToInt32(ctx, &id, id_val);
            return id;
        }
    }
    return -1;
}

static JSValue create_js_audio_obj(JSContext *ctx, int id) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "_id", JS_NewInt32(ctx, id));
    return obj;
}

static int get_js_audio_id(JSContext *ctx, JSValue val) {
    if (JS_IsInt(val)) {
        int id = -1;
        JS_ToInt32(ctx, &id, val);
        return id;
    }
    if (JS_IsPtr(val)) {
        JSValue id_val = JS_GetPropertyStr(ctx, val, "_id");
        if (JS_IsInt(id_val)) {
            int id = -1;
            JS_ToInt32(ctx, &id, id_val);
            return id;
        }
    }
    return -1;
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

JSValue js_is_key_down(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 1) {
        int key = 0;
        JS_ToInt32(ctx, &key, argv[0]);
        if (key >= 0 && key < 256) {
            return JS_NewBool(wagner.keys[key]);
        }
    }
    return JS_NewBool(0);
}

JSValue js_is_key_pressed(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 1) {
        int key = 0;
        JS_ToInt32(ctx, &key, argv[0]);
        if (key >= 0 && key < 256) {
            return JS_NewBool(wagner.keys_pressed[key]);
        }
    }
    return JS_NewBool(0);
}

JSValue js_set_title(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

JSValue js_set_size(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 2) {
        int w = 0, h = 0;
        JS_ToInt32(ctx, &w, argv[0]);
        JS_ToInt32(ctx, &h, argv[1]);
        if (w > 0 && h > 0 && w <= 640 && h <= 480) {
            w_setup(&_wagner_rom.state, NULL, w, h, WAGNER_CFG_BPP, w_scale);
            wagner.width = w; wagner.height = h;
            screen.width = w; screen.height = h;
            screen.stride = w;
        }
    }
    return JS_UNDEFINED;
}

JSValue js_set_scale(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 1) {
        int s = 0;
        JS_ToInt32(ctx, &s, argv[0]);
        if (s > 0 && s <= 8) {
            wagner.scale = s;
        }
    }
    return JS_UNDEFINED;
}

JSValue js_load_image(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 1) {
        JSCStringBuf sbuf;
        const char *path = JS_ToCString(ctx, argv[0], &sbuf);
        if (path && js_image_count < MAX_JS_IMAGES) {
            Image img = load_image(path);
            if (img.pixels) {
                int id = js_image_count++;
                js_images[id] = img;
                return create_js_image_obj(ctx, id, img.width, img.height);
            }
        }
    }
    return JS_NULL;
}

JSValue js_create_image(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 2 && js_image_count < MAX_JS_IMAGES) {
        int w = 0, h = 0;
        JS_ToInt32(ctx, &w, argv[0]);
        JS_ToInt32(ctx, &h, argv[1]);
        if (w > 0 && h > 0) {
            Image img = create_image(w, h);
            if (img.pixels) {
                int id = js_image_count++;
                js_images[id] = img;
                return create_js_image_obj(ctx, id, img.width, img.height);
            }
        }
    }
    return JS_NULL;
}

JSValue js_texture(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 1) {
        int id = get_js_image_id(ctx, argv[0]);
        if (id >= 0 && id < js_image_count) {
            texture(js_images[id]);
        }
    }
    return JS_UNDEFINED;
}

JSValue js_no_texture(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    no_texture();
    return JS_UNDEFINED;
}

JSValue js_color_key(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 1) {
        int col = 0;
        JS_ToInt32(ctx, &col, argv[0]);
        color_key((uint32_t)col);
    }
    return JS_UNDEFINED;
}

JSValue js_no_color_key(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    no_color_key();
    return JS_UNDEFINED;
}

JSValue js_image(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 3) {
        int id = get_js_image_id(ctx, argv[0]);
        if (id >= 0 && id < js_image_count) {
            Image img = js_images[id];
            double x = 0, y = 0, w = img.width, h = img.height;
            JS_ToNumber(ctx, &x, argv[1]);
            JS_ToNumber(ctx, &y, argv[2]);
            if (argc >= 4) JS_ToNumber(ctx, &w, argv[3]);
            if (argc >= 5) JS_ToNumber(ctx, &h, argv[4]);
            
            push();
            translate((float)x, (float)y);
            scale((float)w, (float)h);
            texture(img);
            rect();
            no_texture();
            pop();
        }
    }
    return JS_UNDEFINED;
}

JSValue js_load_gif_anim(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    if (argc >= 1) {
        JSCStringBuf sbuf;
        const char *path = JS_ToCString(ctx, argv[0], &sbuf);
        if (path) {
            Gif g = load_gif_anim(path);
            if (g.frames && g.frame_count > 0) {
                JSValue arr = JS_NewArray(ctx, g.frame_count);
                for (int i = 0; i < g.frame_count; i++) {
                    if (js_image_count < MAX_JS_IMAGES) {
                        int id = js_image_count++;
                        js_images[id] = g.frames[i];
                        JSValue img_obj = create_js_image_obj(ctx, id, g.frames[i].width, g.frames[i].height);
                        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, img_obj);
                    }
                }
                return arr;
            }
        }
    }
    return JS_NULL;
}

/* Wagner Setup & Draw callbacks (invoked by wagner.h) */

void setup() {
    ctx = JS_NewContext(js_heap, sizeof(js_heap), &js_stdlib);
    if (!ctx) return;
    
    JSValue global = JS_GetGlobalObject(ctx);
    
    // Wagner state object
    js_wagnostic_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "width", JS_NewInt32(ctx, wagner.width));
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "height", JS_NewInt32(ctx, wagner.height));
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "mouse_x", JS_NewInt32(ctx, (int)wagner.mouse.x));
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "mouse_y", JS_NewInt32(ctx, (int)wagner.mouse.y));
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "mouse_down", JS_NewBool(wagner.mouse_down));
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "mouse_pressed", JS_NewBool(wagner.mouse_pressed));
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "mouse_released", JS_NewBool(wagner.mouse_released));
    JS_SetPropertyStr(ctx, js_wagnostic_obj, "mouse_wheel", JS_NewInt32(ctx, _wagner_mouse_ptr ? _wagner_mouse_ptr->wheel : 0));
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
    JS_SetPropertyStr(ctx, global, "KEY_W", JS_NewInt32(ctx, KEY_W));
    JS_SetPropertyStr(ctx, global, "KEY_A", JS_NewInt32(ctx, KEY_A));
    JS_SetPropertyStr(ctx, global, "KEY_S", JS_NewInt32(ctx, KEY_S));
    JS_SetPropertyStr(ctx, global, "KEY_D", JS_NewInt32(ctx, KEY_D));
    JS_SetPropertyStr(ctx, global, "KEY_1", JS_NewInt32(ctx, KEY_1));
    JS_SetPropertyStr(ctx, global, "KEY_2", JS_NewInt32(ctx, KEY_2));
    JS_SetPropertyStr(ctx, global, "KEY_3", JS_NewInt32(ctx, KEY_3));

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
    if (!ctx) return;
    
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue wag_obj = JS_GetPropertyStr(ctx, global, "wagner");
    
    // Update mouse and state properties
    JS_SetPropertyStr(ctx, wag_obj, "mouse_x", JS_NewInt32(ctx, (int)wagner.mouse.x));
    JS_SetPropertyStr(ctx, wag_obj, "mouse_y", JS_NewInt32(ctx, (int)wagner.mouse.y));
    JS_SetPropertyStr(ctx, wag_obj, "mouse_down", JS_NewBool(wagner.mouse_down));
    JS_SetPropertyStr(ctx, wag_obj, "mouse_pressed", JS_NewBool(wagner.mouse_pressed));
    JS_SetPropertyStr(ctx, wag_obj, "mouse_released", JS_NewBool(wagner.mouse_released));
    JS_SetPropertyStr(ctx, wag_obj, "mouse_wheel", JS_NewInt32(ctx, _wagner_mouse_ptr ? _wagner_mouse_ptr->wheel : 0));
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
    }
}
