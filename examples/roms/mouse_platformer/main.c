#define WAGNOSTIC_IMPLEMENTATION
#include "wagnostic.h"
#define OLIVEC_IMPLEMENTATION
#include "olive.h"

// Screen dimensions
#define SCREEN_W 320
#define SCREEN_H 240

// Physics constants
#define GRAVITY 0.5f
#define FRICTION 0.85f
#define MOVE_SPEED 3.0f
#define JUMP_FORCE -8.0f

// Fast math constants
#define PI 3.14159265358979f
#define TWO_PI 6.28318530717959f

// Fast sin approximation (Bhaskara I)
static float fast_sin(float x) {
    while (x < 0) x += TWO_PI;
    while (x >= TWO_PI) x -= TWO_PI;
    if (x > PI) {
        float y = x - PI;
        return -16.0f * y * (PI - y) / (5.0f * PI * PI - 4.0f * y * (PI - y));
    }
    return 16.0f * x * (PI - x) / (5.0f * PI * PI - 4.0f * x * (PI - x));
}

// Player structure
typedef struct {
    float x, y;
    float vx, vy;
    int width, height;
    int on_ground;
    int facing_right;
} Player;

// Platform structure
typedef struct {
    int x, y, w, h;
    uint16_t color;
} Platform;

// Coin structure
typedef struct {
    float x, y;
    int collected;
    float bob_offset;
} Coin;

// Particle structure
typedef struct {
    float x, y;
    float vx, vy;
    int life;
    uint16_t color;
} Particle;

// Game state
static Olivec_Canvas _oc;
static Player player;
static Platform platforms[15];
static int num_platforms = 0;
static Coin coins[20];
static int num_coins = 0;
static Particle particles[50];
static int num_particles = 0;
static int camera_x = 0;
static int world_width = 1000;  // World is wider than screen
static int frame_count = 0;
static int score = 0;

// Simple AABB collision check
int check_collision(float x1, float y1, int w1, int h1, 
                   int x2, int y2, int w2, int h2) {
    return (x1 < x2 + w2 &&
            x1 + w1 > x2 &&
            y1 < y2 + h2 &&
            y1 + h1 > y2);
}

// Draw a rectangle
void draw_rect(int x, int y, int w, int h, uint16_t color) {
    uint16_t* pixels = (uint16_t*)_oc.pixels;
    for (int iy = y; iy < y + h; iy++) {
        if (iy < 0 || iy >= SCREEN_H) continue;
        for (int ix = x; ix < x + w; ix++) {
            if (ix >= 0 && ix < SCREEN_W) {
                pixels[iy * SCREEN_W + ix] = color;
            }
        }
    }
}

// Draw player character (simple sprite)
void draw_player() {
    int screen_x = (int)player.x - camera_x;
    int screen_y = (int)player.y;
    
    // Body (blue rectangle)
    draw_rect(screen_x, screen_y, player.width, player.height, W_RGB565(50, 100, 200));
    
    // Eyes (white dots)
    if (player.facing_right) {
        draw_rect(screen_x + 8, screen_y + 4, 3, 3, W_RGB565(255, 255, 255));
        draw_rect(screen_x + 14, screen_y + 4, 3, 3, W_RGB565(255, 255, 255));
    } else {
        draw_rect(screen_x + 4, screen_y + 4, 3, 3, W_RGB565(255, 255, 255));
        draw_rect(screen_x + 10, screen_y + 4, 3, 3, W_RGB565(255, 255, 255));
    }
    
    // Hat (red rectangle on top)
    draw_rect(screen_x + 2, screen_y - 4, 16, 4, W_RGB565(200, 50, 50));
}

// Draw platforms
void draw_platforms() {
    for (int i = 0; i < num_platforms; i++) {
        int screen_x = platforms[i].x - camera_x;
        int screen_y = platforms[i].y;
        
        // Only draw if visible
        if (screen_x + platforms[i].w > 0 && screen_x < SCREEN_W) {
            draw_rect(screen_x, screen_y, platforms[i].w, platforms[i].h, platforms[i].color);
            
            // Add some texture to platforms
            if (platforms[i].h > 10) {
                for (int x = 0; x < platforms[i].w; x += 8) {
                    draw_rect(screen_x + x, screen_y + 2, 4, 2, W_RGB565(80, 60, 40));
                }
            }
        }
    }
}

// Draw coins
void draw_coins() {
    for (int i = 0; i < num_coins; i++) {
        if (coins[i].collected) continue;
        
        int screen_x = (int)coins[i].x - camera_x;
        int screen_y = (int)coins[i].y + (int)(fast_sin(frame_count * 0.1f + coins[i].bob_offset) * 3);
        
        // Only draw if visible
        if (screen_x + 8 > 0 && screen_x < SCREEN_W) {
            // Coin body (yellow circle approximation)
            draw_rect(screen_x, screen_y, 8, 8, W_RGB565(255, 215, 0));
            draw_rect(screen_x + 1, screen_y + 1, 6, 6, W_RGB565(255, 255, 100));
            draw_rect(screen_x + 2, screen_y + 2, 4, 4, W_RGB565(255, 255, 200));
        }
    }
}

// Draw particles
void draw_particles() {
    for (int i = 0; i < num_particles; i++) {
        if (particles[i].life <= 0) continue;
        
        int screen_x = (int)particles[i].x - camera_x;
        int screen_y = (int)particles[i].y;
        
        // Only draw if visible
        if (screen_x >= 0 && screen_x < SCREEN_W && screen_y >= 0 && screen_y < SCREEN_H) {
            int size = particles[i].life / 10 + 1;
            draw_rect(screen_x, screen_y, size, size, particles[i].color);
        }
    }
}

// Draw background (simple parallax)
void draw_background() {
    uint16_t* pixels = (uint16_t*)_oc.pixels;
    // Sky gradient
    for (int y = 0; y < SCREEN_H; y++) {
        uint8_t r = 100 + (y * 50 / SCREEN_H);
        uint8_t g = 150 + (y * 50 / SCREEN_H);
        uint8_t b = 255;
        uint16_t color = W_RGB565(r, g, b);
        for (int x = 0; x < SCREEN_W; x++) {
            pixels[y * SCREEN_W + x] = color;
        }
    }
    
    // Background mountains (parallax)
    for (int i = 0; i < 5; i++) {
        int mx = (i * 200 - camera_x / 3) % 1000;
        if (mx < -100) mx += 1000;
        int my = SCREEN_H - 80;
        int mw = 150;
        int mh = 80;
        
        // Mountain shape (triangle approximation)
        for (int y = 0; y < mh; y++) {
            int width_at_y = (mw * (mh - y)) / mh;
            int start_x = mx + (mw - width_at_y) / 2;
            for (int x = start_x; x < start_x + width_at_y; x++) {
                if (x >= 0 && x < SCREEN_W && my + y >= 0 && my + y < SCREEN_H) {
                    pixels[(my + y) * SCREEN_W + x] = W_RGB565(80, 120, 80);
                }
            }
        }
    }
}

// Draw HUD
void draw_hud() {
    // Score background
    draw_rect(5, 5, 80, 16, W_RGB565(0, 0, 0));
    draw_rect(6, 6, 78, 14, W_RGB565(50, 50, 50));
    
    // Draw score as simple bars
    for (int i = 0; i < score && i < 10; i++) {
        draw_rect(8 + i * 7, 8, 5, 10, W_RGB565(255, 215, 0));
    }
    
    // Draw frame counter (simple dots)
    draw_rect(SCREEN_W - 60, 5, 55, 12, W_RGB565(0, 0, 0));
    draw_rect(SCREEN_W - 59, 6, 53, 10, W_RGB565(50, 50, 50));
    
    for (int i = 0; i < 10; i++) {
        if (frame_count & (1 << i)) {
            draw_rect(SCREEN_W - 55 + i * 5, 8, 3, 6, W_RGB565(0, 200, 0));
        }
    }
    
    // Win message
    if (score >= num_coins) {
        draw_rect(SCREEN_W/2 - 60, SCREEN_H/2 - 20, 120, 40, W_RGB565(0, 0, 0));
        draw_rect(SCREEN_W/2 - 58, SCREEN_H/2 - 18, 116, 36, W_RGB565(0, 100, 0));
        
        // Simple "WIN" text using rectangles
        // W
        draw_rect(SCREEN_W/2 - 40, SCREEN_H/2 - 10, 4, 20, W_RGB565(255, 255, 255));
        draw_rect(SCREEN_W/2 - 32, SCREEN_H/2 - 10, 4, 20, W_RGB565(255, 255, 255));
        draw_rect(SCREEN_W/2 - 36, SCREEN_H/2 + 5, 4, 5, W_RGB565(255, 255, 255));
        
        // I
        draw_rect(SCREEN_W/2 - 20, SCREEN_H/2 - 10, 4, 20, W_RGB565(255, 255, 255));
        
        // N
        draw_rect(SCREEN_W/2 - 10, SCREEN_H/2 - 10, 4, 20, W_RGB565(255, 255, 255));
        draw_rect(SCREEN_W/2 - 2, SCREEN_H/2 - 10, 4, 20, W_RGB565(255, 255, 255));
        draw_rect(SCREEN_W/2 + 2, SCREEN_H/2 - 5, 4, 5, W_RGB565(255, 255, 255));
        draw_rect(SCREEN_W/2 + 6, SCREEN_H/2, 4, 5, W_RGB565(255, 255, 255));
    }
}

// Create particle effect
void create_particles(float x, float y, uint16_t color, int count) {
    for (int i = 0; i < count && num_particles < 50; i++) {
        particles[num_particles].x = x;
        particles[num_particles].y = y;
        particles[num_particles].vx = (float)((frame_count + i) % 10 - 5) * 0.5f;
        particles[num_particles].vy = (float)((frame_count + i * 7) % 10 - 5) * 0.5f;
        particles[num_particles].life = 30;
        particles[num_particles].color = color;
        num_particles++;
    }
}

// Initialize game
void winit() {
    w_setup("Mouse Platformer", SCREEN_W, SCREEN_H, 16, 4, 0);
    _oc = olivec_canvas(w_vram, SCREEN_W, SCREEN_H, SCREEN_W, 16);
    
    // Initialize player
    player.x = 100;
    player.y = 150;
    player.vx = 0;
    player.vy = 0;
    player.width = 20;
    player.height = 24;
    player.on_ground = 0;
    player.facing_right = 1;
    
    // Create platforms
    // Ground
    platforms[num_platforms++] = (Platform){0, 200, 1000, 40, W_RGB565(100, 70, 50)};
    
    // Floating platforms
    platforms[num_platforms++] = (Platform){100, 160, 80, 20, W_RGB565(80, 160, 80)};
    platforms[num_platforms++] = (Platform){250, 130, 100, 20, W_RGB565(80, 160, 80)};
    platforms[num_platforms++] = (Platform){400, 100, 80, 20, W_RGB565(80, 160, 80)};
    platforms[num_platforms++] = (Platform){550, 140, 120, 20, W_RGB565(80, 160, 80)};
    platforms[num_platforms++] = (Platform){700, 80, 80, 20, W_RGB565(80, 160, 80)};
    platforms[num_platforms++] = (Platform){850, 120, 100, 20, W_RGB565(80, 160, 80)};
    
    // Walls
    platforms[num_platforms++] = (Platform){0, 0, 20, 200, W_RGB565(120, 100, 80)};
    platforms[num_platforms++] = (Platform){980, 0, 20, 200, W_RGB565(120, 100, 80)};
    
    // Create coins
    coins[num_coins++] = (Coin){150, 140, 0, 0.0f};
    coins[num_coins++] = (Coin){300, 110, 0, 1.0f};
    coins[num_coins++] = (Coin){450, 80, 0, 2.0f};
    coins[num_coins++] = (Coin){600, 120, 0, 3.0f};
    coins[num_coins++] = (Coin){750, 60, 0, 4.0f};
    coins[num_coins++] = (Coin){900, 100, 0, 5.0f};
}

// Update game state
__attribute__((visibility("default")))
void wupdate() {
    frame_count++;
    
    // Handle mouse input
    int mouse_x = w_mouse_x;
    int mouse_y = w_mouse_y;
    int mouse_clicked = w_mouse_buttons & 1;
    
    // Convert mouse to world coordinates
    int target_x = mouse_x + camera_x;
    int target_y = mouse_y;
    
    // Move player toward mouse click
    if (mouse_clicked) {
        float dx = target_x - player.x;
        float dy = target_y - player.y;
        float dist = 0;
        
        // Simple distance calculation (no sqrt for speed)
        if (dx > 0) dist += dx; else dist -= dx;
        if (dy > 0) dist += dy; else dist -= dy;
        
        if (dist > 2) {
            // Normalize and apply speed
            float speed = MOVE_SPEED;
            if (dist < 50) speed = MOVE_SPEED * 0.5f;  // Slow down near target
            
            if (dx > 0) player.vx += speed * 0.3f;
            else player.vx -= speed * 0.3f;
            
            if (dy > 0 && !player.on_ground) player.vy += speed * 0.2f;
            else if (dy < 0 && player.on_ground) {
                // Jump if clicking above
                player.vy = JUMP_FORCE;
                player.on_ground = 0;
            }
            
            // Face direction
            if (dx > 0) player.facing_right = 1;
            else player.facing_right = 0;
        }
    }
    
    // Apply gravity
    player.vy += GRAVITY;
    
    // Apply friction
    player.vx *= FRICTION;
    
    // Update position
    float new_x = player.x + player.vx;
    float new_y = player.y + player.vy;
    
    // Collision detection with platforms
    player.on_ground = 0;
    
    for (int i = 0; i < num_platforms; i++) {
        Platform *p = &platforms[i];
        
        // Check horizontal collision
        if (check_collision(new_x, player.y, player.width, player.height,
                           p->x, p->y, p->w, p->h)) {
            if (player.vx > 0) {
                new_x = p->x - player.width;
            } else if (player.vx < 0) {
                new_x = p->x + p->w;
            }
            player.vx = 0;
        }
        
        // Check vertical collision
        if (check_collision(player.x, new_y, player.width, player.height,
                           p->x, p->y, p->w, p->h)) {
            if (player.vy > 0) {
                new_y = p->y - player.height;
                player.on_ground = 1;
            } else if (player.vy < 0) {
                new_y = p->y + p->h;
            }
            player.vy = 0;
        }
    }
    
    // Update player position
    player.x = new_x;
    player.y = new_y;
    
    // Keep player in world bounds
    if (player.x < 0) player.x = 0;
    if (player.x > world_width - player.width) player.x = world_width - player.width;
    if (player.y < 0) player.y = 0;
    if (player.y > SCREEN_H) {
        // Respawn if fell off
        player.x = 100;
        player.y = 150;
        player.vx = 0;
        player.vy = 0;
    }
    
    // Check coin collection
    for (int i = 0; i < num_coins; i++) {
        if (coins[i].collected) continue;
        
        float coin_x = coins[i].x;
        float coin_y = coins[i].y + (float)(fast_sin(frame_count * 0.1f + coins[i].bob_offset) * 3);
        
        if (check_collision(player.x, player.y, player.width, player.height,
                           (int)coin_x, (int)coin_y, 8, 8)) {
            coins[i].collected = 1;
            score++;
            create_particles(coin_x, coin_y, W_RGB565(255, 215, 0), 10);
        }
    }
    
    // Update particles
    for (int i = 0; i < num_particles; i++) {
        if (particles[i].life <= 0) continue;
        
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].life--;
        
        // Remove dead particles
        if (particles[i].life <= 0) {
            particles[i] = particles[num_particles - 1];
            num_particles--;
            i--;
        }
    }
    
    // Update camera to follow player
    camera_x = (int)player.x - SCREEN_W / 2;
    if (camera_x < 0) camera_x = 0;
    if (camera_x > world_width - SCREEN_W) camera_x = world_width - SCREEN_W;
    
    // Draw everything
    draw_background();
    draw_platforms();
    draw_coins();
    draw_player();
    draw_particles();
    draw_hud();
    
    // Draw mouse cursor
    draw_rect(mouse_x - 2, mouse_y - 2, 5, 5, W_RGB565(255, 255, 0));
    if (mouse_clicked) {
        draw_rect(mouse_x - 4, mouse_y - 4, 9, 9, W_RGB565(255, 0, 0));
    }
    
    w_redraw();
}
