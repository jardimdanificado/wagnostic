#!/usr/bin/env python3
import os
import subprocess

STDLIB_C = os.path.join("mquickjs", "mqjs_stdlib.c")

if not os.path.exists(STDLIB_C):
    print(f"Error: {STDLIB_C} not found.")
    exit(1)

# Reset mqjs_stdlib.c to original via git checkout in mquickjs folder
subprocess.run(["git", "checkout", "mqjs_stdlib.c"], cwd="mquickjs", check=True)

with open(STDLIB_C, "r") as f:
    content = f.read()

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
    JS_CFUNC_DEF("is_key_down", 1, js_is_key_down),
    JS_CFUNC_DEF("key_down", 1, js_is_key_down),
    JS_CFUNC_DEF("is_key_pressed", 1, js_is_key_pressed),
    JS_CFUNC_DEF("key_pressed", 1, js_is_key_pressed),
    JS_CFUNC_DEF("set_title", 1, js_set_title),
    JS_CFUNC_DEF("set_size", 2, js_set_size),
    JS_CFUNC_DEF("set_scale", 1, js_set_scale),
    JS_CFUNC_DEF("load_image", 1, js_load_image),
    JS_CFUNC_DEF("create_image", 2, js_create_image),
    JS_CFUNC_DEF("texture", 1, js_texture),
    JS_CFUNC_DEF("no_texture", 0, js_no_texture),
    JS_CFUNC_DEF("color_key", 1, js_color_key),
    JS_CFUNC_DEF("no_color_key", 0, js_no_color_key),
    JS_CFUNC_DEF("image", 5, js_image),
"""

target = 'JS_CFUNC_DEF("clearTimeout", 1, js_clearTimeout),'
if target in content:
    content = content.replace(target, target + "\n" + wagner_funcs)
    with open(STDLIB_C, "w") as f:
        f.write(content)
    print("Patched mqjs_stdlib.c with all Wagner C-function definitions.")
else:
    print("Target clearTimeout not found in mqjs_stdlib.c")
