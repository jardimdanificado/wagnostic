#ifndef OLIVE_H_
#define OLIVE_H_

// this is adapted from the original olive.c

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// ============================================
// TYPES & STRUCTURES
// ============================================

typedef struct {
    size_t width, height;
    const char *glyphs;
} Olivec_Font;

typedef struct {
    void *pixels;
    size_t width;
    size_t height;
    size_t stride;
    uint8_t bpp; // 8, 16, or 32
} Olivec_Canvas;

typedef struct {
    int x1, x2;
    int y1, y2;
    int ox1, ox2;
    int oy1, oy2;
} Olivec_Normalized_Rect;

#ifndef OLIVECDEF
#define OLIVECDEF static inline
#endif

#define OLIVEC_SWAP(T, a, b) do { T t = a; a = b; b = t; } while (0)
#define OLIVEC_SIGN(T, x) ((T)((x) > 0) - (T)((x) < 0))
#define OLIVEC_ABS(T, x) (OLIVEC_SIGN(T, x)*(x))

// ============================================
// DEFAULT FONT DATA
// ============================================

#define OLIVEC_DEFAULT_FONT_HEIGHT 6
#define OLIVEC_DEFAULT_FONT_WIDTH 6
static char olivec_default_glyphs[128][OLIVEC_DEFAULT_FONT_HEIGHT][OLIVEC_DEFAULT_FONT_WIDTH] = {
    ['a']={{0,0,0,0,0},{0,1,1,0,0},{0,0,0,1,0},{0,1,1,1,0},{1,0,0,1,0},{0,1,1,1,0}},
    ['b']={{1,0,0,0,0},{1,1,1,0,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{1,1,1,0,0}},
    ['c']={{0,0,0,0,0},{0,1,1,0,0},{1,0,0,1,0},{1,0,0,0,0},{1,0,0,1,0},{0,1,1,0,0}},
    ['d']={{0,0,0,1,0},{0,1,1,1,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{0,1,1,1,0}},
    ['e']={{0,0,0,0,0},{0,1,1,0,0},{1,0,0,1,0},{1,1,1,1,0},{1,0,0,0,0},{0,1,1,1,0}},
    ['f']={{0,0,1,1,0},{0,1,0,0,0},{1,1,1,1,0},{0,1,0,0,0},{0,1,0,0,0},{0,1,0,0,0}},
    ['g']={{0,1,1,1,0},{1,0,0,1,0},{1,0,0,1,0},{0,1,1,1,0},{0,0,0,1,0},{0,1,1,0,0}},
    ['h']={{1,0,0,0,0},{1,1,1,0,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0}},
    ['i']={{0,0,1,0,0},{0,0,0,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0}},
    ['j']={{0,0,1,0,0},{0,0,0,0,0},{0,0,1,0,0},{0,0,1,0,0},{1,0,1,0,0},{0,1,1,0,0}},
    ['k']={{1,0,0,0,0},{1,0,0,1,0},{1,0,1,0,0},{1,1,0,0,0},{1,0,1,0,0},{1,0,0,1,0}},
    ['l']={{0,1,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,1,1,1,0}},
    ['m']={{0,0,0,0,0},{0,1,0,1,1},{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1}},
    ['n']={{0,0,0,0,0},{0,1,1,1,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0}},
    ['o']={{0,0,0,0,0},{0,1,1,0,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{0,1,1,0,0}},
    ['p']={{1,1,1,0,0},{1,0,0,1,0},{1,0,0,1,0},{1,1,1,0,0},{1,0,0,0,0},{1,0,0,0,0}},
    ['q']={{0,1,1,1,0},{1,0,0,1,0},{1,0,0,1,0},{0,1,1,1,0},{0,0,0,1,0},{0,0,0,1,0}},
    ['r']={{0,0,0,0,0},{1,0,1,1,0},{1,1,0,0,1},{1,0,0,0,0},{1,0,0,0,0},{1,0,0,0,0}},
    ['s']={{0,0,0,0,0},{0,1,1,1,0},{1,0,0,0,0},{1,1,1,1,0},{0,0,0,1,0},{1,1,1,0,0}},
    ['t']={{0,1,0,0,0},{0,1,0,0,0},{1,1,1,1,0},{0,1,0,0,0},{0,1,0,1,0},{0,1,1,0,0}},
    ['u']={{0,0,0,0,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{0,1,1,1,0}},
    ['v']={{0,0,0,0,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{0,1,1,0,0}},
    ['w']={{0,0,0,0,0},{1,0,0,0,1},{1,0,1,0,1},{1,0,1,0,1},{1,0,1,0,1},{0,1,1,1,1}},
    ['x']={{0,0,0,0,0},{1,0,1,0,0},{1,0,1,0,0},{0,1,0,0,0},{1,0,1,0,0},{1,0,1,0,0}},
    ['y']={{0,0,0,0,0},{1,0,1,0,0},{1,0,1,0,0},{1,0,1,0,0},{0,1,0,0,0},{0,1,0,0,0}},
    ['z']={{0,0,0,0,0},{1,1,1,1,0},{0,0,0,1,0},{0,1,1,0,0},{1,0,0,0,0},{1,1,1,1,0}},
    ['0']={{0,1,1,0,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{1,0,0,1,0},{0,1,1,0,0}},
    ['1']={{0,0,1,0,0},{0,1,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,0,1,0,0},{0,1,1,1,0}},
    ['2']={{0,1,1,0,0},{1,0,0,1,0},{0,0,0,1,0},{0,1,1,0,0},{1,0,0,0,0},{1,1,1,1,0}},
    ['3']={{0,1,1,0,0},{1,0,0,1,0},{0,0,1,0,0},{0,0,0,1,0},{1,0,0,1,0},{0,1,1,0,0}},
    ['4']={{0,0,1,1,0},{0,1,0,1,0},{1,0,0,1,0},{1,1,1,1,1},{0,0,0,1,0},{0,0,0,1,0}},
    ['5']={{1,1,1,0,0},{1,0,0,0,0},{1,1,1,0,0},{0,0,0,1,0},{1,0,0,1,0},{0,1,1,0,0}},
    ['6']={{0,1,1,0,0},{1,0,0,0,0},{1,1,1,0,0},{1,0,0,1,0},{1,0,0,1,0},{0,1,1,0,0}},
    ['7']={{1,1,1,1,0},{0,0,0,1,0},{0,0,1,0,0},{0,1,0,0,0},{0,1,0,0,0},{0,1,0,0,0}},
    ['8']={{0,1,1,0,0},{1,0,0,1,0},{0,1,1,0,0},{1,0,0,1,0},{1,0,0,1,0},{0,1,1,0,0}},
    ['9']={{0,1,1,0,0},{1,0,0,1,0},{1,0,0,1,0},{0,1,1,1,0},{0,0,0,1,0},{0,1,1,0,0}},
};
static Olivec_Font olivec_default_font = { .glyphs = &olivec_default_glyphs[0][0][0], .width = 6, .height = 6 };

// ============================================
// IMPLEMENTATION
// ============================================

#ifdef OLIVEC_IMPLEMENTATION

OLIVECDEF bool olivec_normalize_rect(int x, int y, int w, int h, size_t cw, size_t ch, Olivec_Normalized_Rect *nr) {
    if (w == 0 || h == 0) return false;
    nr->ox1 = x; nr->oy1 = y;
    nr->ox2 = x + OLIVEC_SIGN(int, w)*(OLIVEC_ABS(int, w) - 1);
    if (nr->ox1 > nr->ox2) OLIVEC_SWAP(int, nr->ox1, nr->ox2);
    nr->oy2 = y + OLIVEC_SIGN(int, h)*(OLIVEC_ABS(int, h) - 1);
    if (nr->oy1 > nr->oy2) OLIVEC_SWAP(int, nr->oy1, nr->oy2);
    if (nr->ox1 >= (int)cw || nr->ox2 < 0 || nr->oy1 >= (int)ch || nr->oy2 < 0) return false;
    nr->x1 = nr->ox1 < 0 ? 0 : nr->ox1;
    nr->y1 = nr->oy1 < 0 ? 0 : nr->oy1;
    nr->x2 = nr->ox2 >= (int)cw ? (int)cw - 1 : nr->ox2;
    nr->y2 = nr->oy2 >= (int)ch ? (int)ch - 1 : nr->oy2;
    return true;
}

OLIVECDEF Olivec_Canvas olivec_canvas(void *pixels, size_t width, size_t height, size_t stride, uint8_t bpp) {
    return (Olivec_Canvas){ .pixels = pixels, .width = width, .height = height, .stride = stride, .bpp = bpp };
}

OLIVECDEF Olivec_Canvas olivec_subcanvas(Olivec_Canvas oc, int x, int y, int w, int h) {
    Olivec_Normalized_Rect nr;
    if (!olivec_normalize_rect(x, y, w, h, oc.width, oc.height, &nr)) return (Olivec_Canvas){0};
    void *pixels = (uint8_t*)oc.pixels + (nr.y1 * oc.stride + nr.x1) * (oc.bpp / 8);
    return (Olivec_Canvas){ .pixels = pixels, .width = nr.x2 - nr.x1 + 1, .height = nr.y2 - nr.y1 + 1, .stride = oc.stride, .bpp = oc.bpp };
}

static inline void olivec_set_pixel(Olivec_Canvas oc, int x, int y, uint32_t color) {
    if (x < 0 || (size_t)x >= oc.width || y < 0 || (size_t)y >= oc.height) return;
    if (oc.bpp == 32) ((uint32_t*)oc.pixels)[y * oc.stride + x] = color;
    else if (oc.bpp == 16) { if (color != 0) ((uint16_t*)oc.pixels)[y * oc.stride + x] = (uint16_t)color; }
    else ((uint8_t*)oc.pixels)[y * oc.stride + x] = (uint8_t)color;
}

static inline uint32_t olivec_get_pixel(Olivec_Canvas oc, int x, int y) {
    if (x < 0 || (size_t)x >= oc.width || y < 0 || (size_t)y >= oc.height) return 0;
    if (oc.bpp == 32) return ((uint32_t*)oc.pixels)[y * oc.stride + x];
    if (oc.bpp == 16) return ((uint16_t*)oc.pixels)[y * oc.stride + x];
    return ((uint8_t*)oc.pixels)[y * oc.stride + x];
}

OLIVECDEF void olivec_fill(Olivec_Canvas oc, uint32_t color) {
    size_t n = oc.width * oc.height;
    if (oc.bpp == 32) { uint32_t *p = (uint32_t*)oc.pixels; while(n--) *p++ = color; }
    else if (oc.bpp == 16) { uint16_t *p = (uint16_t*)oc.pixels; while(n--) *p++ = (uint16_t)color; }
    else { uint8_t *p = (uint8_t*)oc.pixels; while(n--) *p++ = (uint8_t)color; }
}

OLIVECDEF void olivec_rect(Olivec_Canvas oc, int x, int y, int w, int h, uint32_t color) {
    Olivec_Normalized_Rect nr;
    if (!olivec_normalize_rect(x, y, w, h, oc.width, oc.height, &nr)) return;
    for (int iy = nr.y1; iy <= nr.y2; ++iy) {
        for (int ix = nr.x1; ix <= nr.x2; ++ix) olivec_set_pixel(oc, ix, iy, color);
    }
}

OLIVECDEF void olivec_circle(Olivec_Canvas oc, int cx, int cy, int r, uint32_t color) {
    int x = r, y = 0, err = 0;
    while (x >= y) {
        olivec_set_pixel(oc, cx+x, cy+y, color); olivec_set_pixel(oc, cx+y, cy+x, color);
        olivec_set_pixel(oc, cx-y, cy+x, color); olivec_set_pixel(oc, cx-x, cy+y, color);
        olivec_set_pixel(oc, cx-x, cy-y, color); olivec_set_pixel(oc, cx-y, cy-x, color);
        olivec_set_pixel(oc, cx+y, cy-x, color); olivec_set_pixel(oc, cx+x, cy-y, color);
        if (err <= 0) { y += 1; err += 2*y + 1; }
        if (err > 0) { x -= 1; err -= 2*x + 1; }
    }
}

OLIVECDEF void olivec_line(Olivec_Canvas oc, int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = OLIVEC_ABS(int, x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -OLIVEC_ABS(int, y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        olivec_set_pixel(oc, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

OLIVECDEF void olivec_text(Olivec_Canvas oc, const char *text, int x, int y, Olivec_Font font, size_t size, uint32_t color) {
    for (size_t i = 0; text[i] != '\0'; ++i) {
        int gx = x + (int)(i * font.width * size);
        for (size_t j = 0; j < font.height; ++j) {
            for (size_t k = 0; k < font.width; ++k) {
                if (font.glyphs[text[i] * font.width * font.height + j * font.width + k]) {
                    olivec_rect(oc, gx + (int)(k * size), y + (int)(j * size), (int)size, (int)size, color);
                }
            }
        }
    }
}

OLIVECDEF bool olivec_barycentric(int x1, int y1, int x2, int y2, int x3, int y3, int xp, int yp, int *u1, int *u2, int *det) {
    *det = ((x1 - x3)*(y2 - y3) - (x2 - x3)*(y1 - y3));
    *u1  = ((y2 - y3)*(xp - x3) + (x3 - x2)*(yp - y3));
    *u2  = ((y3 - y1)*(xp - x3) + (x1 - x3)*(yp - y3));
    int u3 = *det - *u1 - *u2;
    return ((OLIVEC_SIGN(int, *u1) == OLIVEC_SIGN(int, *det) || *u1 == 0) &&
            (OLIVEC_SIGN(int, *u2) == OLIVEC_SIGN(int, *det) || *u2 == 0) &&
            (OLIVEC_SIGN(int, u3) == OLIVEC_SIGN(int, *det) || u3 == 0));
}

OLIVECDEF void olivec_triangle(Olivec_Canvas oc, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    int lx = x1, hx = x1, ly = y1, hy = y1;
    if (x2 < lx) lx = x2; if (x3 < lx) lx = x3; if (x2 > hx) hx = x2; if (x3 > hx) hx = x3;
    if (y2 < ly) ly = y2; if (y3 < ly) ly = y3; if (y2 > hy) hy = y2; if (y3 > hy) hy = y3;
    if (lx < 0) lx = 0; if (hx >= (int)oc.width) hx = (int)oc.width - 1;
    if (ly < 0) ly = 0; if (hy >= (int)oc.height) hy = (int)oc.height - 1;
    for (int y = ly; y <= hy; ++y) {
        for (int x = lx; x <= hx; ++x) {
            int u1, u2, det;
            if (olivec_barycentric(x1, y1, x2, y2, x3, y3, x, y, &u1, &u2, &det)) olivec_set_pixel(oc, x, y, color);
        }
    }
}

OLIVECDEF void olivec_triangle3uv(Olivec_Canvas oc, int x1, int y1, int x2, int y2, int x3, int y3, float tx1, float ty1, float tx2, float ty2, float tx3, float ty3, float z1, float z2, float z3, Olivec_Canvas texture) {
    int lx = x1, hx = x1, ly = y1, hy = y1;
    if (x2 < lx) lx = x2; if (x3 < lx) lx = x3; if (x2 > hx) hx = x2; if (x3 > hx) hx = x3;
    if (y2 < ly) ly = y2; if (y3 < ly) ly = y3; if (y2 > hy) hy = y2; if (y3 > hy) hy = y3;
    if (lx < 0) lx = 0; if (hx >= (int)oc.width) hx = (int)oc.width - 1;
    if (ly < 0) ly = 0; if (hy >= (int)oc.height) hy = (int)oc.height - 1;
    for (int y = ly; y <= hy; ++y) {
        for (int x = lx; x <= hx; ++x) {
            int u1, u2, det;
            if (olivec_barycentric(x1, y1, x2, y2, x3, y3, x, y, &u1, &u2, &det)) {
                float u3 = (float)(det - u1 - u2) / det, fu1 = (float)u1 / det, fu2 = (float)u2 / det;
                float z = z1*fu1 + z2*fu2 + z3*u3;
                float tx = tx1*fu1 + tx2*fu2 + tx3*u3;
                float ty = ty1*fu1 + ty2*fu2 + ty3*u3;
                olivec_set_pixel(oc, x, y, olivec_get_pixel(texture, (int)(tx/z*texture.width), (int)(ty/z*texture.height)));
            }
        }
    }
}

OLIVECDEF void olivec_sprite_copy(Olivec_Canvas dst, int x, int y, int w, int h, Olivec_Canvas src) {
    for (int iy = 0; iy < h; ++iy) {
        for (int ix = 0; ix < w; ++ix) {
            int sx = ix * (int)src.width / w;
            int sy = iy * (int)src.height / h;
            olivec_set_pixel(dst, x + ix, y + iy, olivec_get_pixel(src, sx, sy));
        }
    }
}

#endif // OLIVEC_IMPLEMENTATION
#endif // OLIVE_H_
