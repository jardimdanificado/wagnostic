#include "wagnostic.h"
#define OLIVEC_IMPLEMENTATION
#include "olive.h"
#include "data/sprites.h"

static Olivec_Canvas _oc;
static Olivec_Canvas _sheet;

#define SPRITE_FLOOR   10
#define SPRITE_WALL    25
#define SPRITE_PLAYER  1
#define SPRITE_MONSTER 45
#define MAP_W 20
#define MAP_H 15

static uint8_t map[MAP_H][MAP_W];
typedef struct { int x, y, hp; } Entity;
static Entity player;
#define MAX_MONSTERS 10
static Entity monsters[MAX_MONSTERS];
static int num_monsters = 0;
static uint32_t _seed = 12345;
static uint32_t rand_u32() { _seed = _seed * 1103515245 + 12345; return (_seed / 65536) % 32768; }
static int rand_range(int min, int max) { if (max <= min) return min; return min + (rand_u32() % (max - min)); }

typedef struct { int x, y, w, h; } Room;
static Room rooms[15];
static int num_rooms = 0;

static void build_room(Room *r) {
    for (int y = r->y; y < r->y + r->h; y++)
        for (int x = r->x; x < r->x + r->w; x++)
            if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) map[y][x] = 0;
}

static void generate_map() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) map[y][x] = 1;
    num_rooms = 0;
    for (int i = 0; i < 10; i++) {
        Room r; r.w = rand_range(4, 8); r.h = rand_range(4, 6);
        r.x = rand_range(1, MAP_W - r.w - 1); r.y = rand_range(1, MAP_H - r.h - 1);
        bool overlap = false;
        for (int j = 0; j < num_rooms; j++) {
            if (r.x < rooms[j].x + rooms[j].w + 1 && r.x + r.w + 1 > rooms[j].x &&
                r.y < rooms[j].y + rooms[j].h + 1 && r.y + r.h + 1 > rooms[j].y) { overlap = true; break; }
        }
        if (!overlap) {
            build_room(&r);
            if (num_rooms > 0) {
                int cx1 = rooms[num_rooms-1].x + rooms[num_rooms-1].w/2;
                int cy1 = rooms[num_rooms-1].y + rooms[num_rooms-1].h/2;
                int cx2 = r.x + r.w/2; int cy2 = r.y + r.h/2;
                for (int x = (cx1 < cx2 ? cx1 : cx2); x <= (cx1 < cx2 ? cx2 : cx1); x++) map[cy1][x] = 0;
                for (int y = (cy1 < cy2 ? cy1 : cy2); y <= (cy1 < cy2 ? cy2 : cy1); y++) map[y][cx2] = 0;
            }
            rooms[num_rooms++] = r;
        }
    }
    player.x = rooms[0].x + rooms[0].w / 2; player.y = rooms[0].y + rooms[0].h / 2; player.hp = 10;
    num_monsters = 0;
    for (int i = 1; i < num_rooms; i++) {
        if (num_monsters < MAX_MONSTERS) {
            monsters[num_monsters].x = rooms[i].x + rooms[i].w / 2;
            monsters[num_monsters].y = rooms[i].y + rooms[i].h / 2;
            monsters[num_monsters].hp = 3; num_monsters++;
        }
    }
}

static uint32_t prev_buttons = 0;

void winit() {
    w_setup("Wagnostic SDK - Roguelike", 320, 240, 16, 4, 8);
    _oc = olivec_canvas(W_FB_PTR, 320, 240, 320, 16);
    _sheet = olivec_canvas((uint16_t*)image_raw, image_width, image_height, image_width, 16);
    generate_map();
}

__attribute__((visibility("default")))
void wupdate() {
    uint32_t pressed = W_SYS->gamepad_buttons & ~prev_buttons;
    prev_buttons = W_SYS->gamepad_buttons;
    
    int dx = 0, dy = 0;
    if (pressed & W_BTN_LEFT) dx = -1; if (pressed & W_BTN_RIGHT) dx = 1;
    if (pressed & W_BTN_UP) dy = -1; if (pressed & W_BTN_DOWN) dy = 1;
    
    if (dx != 0 || dy != 0) {
        int nx = player.x + dx; int ny = player.y + dy;
        if (nx >= 0 && nx < MAP_W && ny >= 0 && ny < MAP_H && map[ny][nx] == 0) {
            bool monster_there = false;
            for (int i = 0; i < num_monsters; i++) {
                if (monsters[i].hp > 0 && monsters[i].x == nx && monsters[i].y == ny) { monsters[i].hp--; monster_there = true; break; }
            }
            if (!monster_there) { player.x = nx; player.y = ny; }
            for (int i = 0; i < num_monsters; i++) {
                if (monsters[i].hp > 0) {
                    int mdx = (player.x > monsters[i].x) ? 1 : (player.x < monsters[i].x ? -1 : 0);
                    int mdy = (player.y > monsters[i].y) ? 1 : (player.y < monsters[i].y ? -1 : 0);
                    int mnx = monsters[i].x + mdx; int mny = monsters[i].y + mdy;
                    if (mnx == player.x && mny == player.y) { player.hp--; }
                    else if (map[mny][mnx] == 0) { monsters[i].x = mnx; monsters[i].y = mny; }
                }
            }
        }
    }

    olivec_fill(_oc, W_RGB565(10, 10, 15));
    int off_x = (320 - MAP_W * 16) / 2, off_y = (240 - MAP_H * 16) / 2;
    
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            int id = (map[y][x] == 1) ? SPRITE_WALL : SPRITE_FLOOR;
            olivec_sprite_copy(_oc, off_x + x*16, off_y + y*16, 16, 16, olivec_subcanvas(_sheet, (id%16)*16, (id/16)*16, 16, 16));
        }
    }
    
    olivec_sprite_copy(_oc, off_x + player.x*16, off_y + player.y*16, 16, 16, olivec_subcanvas(_sheet, (SPRITE_PLAYER%16)*16, (SPRITE_PLAYER/16)*16, 16, 16));
    
    for (int m = 0; m < num_monsters; m++) {
        if (monsters[m].hp > 0) {
            olivec_sprite_copy(_oc, off_x + monsters[m].x*16, off_y + monsters[m].y*16, 16, 16, olivec_subcanvas(_sheet, (SPRITE_MONSTER%16)*16, (SPRITE_MONSTER/16)*16, 16, 16));
        }
    }
    
    w_redraw();
}
