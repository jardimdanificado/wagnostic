#!/usr/bin/env python3
import os

STDLIB_C = os.path.join("mquickjs", "mqjs_stdlib.c")

if not os.path.exists(STDLIB_C):
    print(f"Error: {STDLIB_C} not found.")
    exit(1)

with open(STDLIB_C, "r") as f:
    content = f.read()

if "js_push" in content:
    print("mqjs_stdlib.c already patched.")
    exit(0)

wagner_funcs = """
    JS_CFUNC_DEF("push", 0, js_push),
    JS_CFUNC_DEF("pop", 0, js_pop),
    JS_CFUNC_DEF("translate", 2, js_translate),
    JS_CFUNC_DEF("rotate", 1, js_rotate),
    JS_CFUNC_DEF("scale", 2, js_scale),
    JS_CFUNC_DEF("fill", 4, js_fill),
    JS_CFUNC_DEF("no_fill", 0, js_no_fill),
    JS_CFUNC_DEF("stroke", 4, js_stroke),
    JS_CFUNC_DEF("no_stroke", 0, js_no_stroke),
    JS_CFUNC_DEF("clear", 0, js_clear),
    JS_CFUNC_DEF("rect", 4, js_rect),
    JS_CFUNC_DEF("circle", 3, js_circle),
    JS_CFUNC_DEF("triangle", 6, js_triangle),
    JS_CFUNC_DEF("triangle_pts", 6, js_triangle_pts),
    JS_CFUNC_DEF("line", 4, js_line),
    JS_CFUNC_DEF("pixel", 2, js_pixel),
    JS_CFUNC_DEF("text", 3, js_text),
    JS_CFUNC_DEF("rgb", 3, js_rgb),
    JS_CFUNC_DEF("rgba", 4, js_rgba),
    JS_CFUNC_DEF("play_tone", 3, js_play_tone),
    JS_CFUNC_DEF("play_noise", 2, js_play_noise),
    JS_CFUNC_DEF("stop_all_sounds", 0, js_stop_all_sounds),
"""

target = 'JS_CFUNC_DEF("clearTimeout", 1, js_clearTimeout),'
if target in content:
    content = content.replace(target, target + "\n" + wagner_funcs)
    with open(STDLIB_C, "w") as f:
        f.write(content)
    print("Patched mqjs_stdlib.c with Wagner C-function definitions.")
else:
    print("Target clearTimeout not found in mqjs_stdlib.c")
