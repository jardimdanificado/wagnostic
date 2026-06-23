/**
 * WagnO Example - Simple Platformer with Sprites & Audio
 * 
 * This example demonstrates:
 * - Sprite rendering (player and coin images)
 * - Audio playback (coin pickup sound)
 * - Mouse-controlled movement
 * - Platformer physics and collision
 */

#define WAGNO_IMPLEMENTATION
#include "wagno.h"

// Include sprite data
#include "player_sprite.c"
#include "coin_sprite.c"

// Include audio data
#include "coin_sound.c"

// Game constants
#define GRAVITY 0.5f
#define JUMP_FORCE -10.0f
#define MOVE_SPEED 3.0f
#define GROUND_Y 200

// Player state
typedef struct {
    float x, y;
    float vx, vy;
    int width, height;
    bool on_ground;
    bool facing_right;
} Player;

static Player player;

// Platform state
typedef struct {
    int x, y, w, h;
} Platform;

static Platform platforms[10];
static int num_platforms = 0;

// Coin state
typedef struct {
    float x, y;
    bool collected;
    float bob_offset;
} Coin;

static Coin coins[20];
static int num_coins = 0;

// Game state
static int score = 0;
static int camera_x = 0;
static int world_width = 1000;

// Sprites
static WagnoImage player_sprite;
static WagnoImage coin_sprite;

// Audio state
static uint8_t* audio_buffer = NULL;
static int audio_write_pos = 0;

// ============================================
// AUDIO FUNCTIONS
// ============================================

static void init_audio() {
    // Setup audio buffer in WASM memory
    W_SYS->audio_size = 8192;
    W_SYS->audio_sample_rate = 44100;
    W_SYS->audio_bpp = 1;  // 8-bit mono
    W_SYS->audio_channels = 1;
    W_SYS->audio_write = 0;
    W_SYS->audio_read = 0;
    
    // Signal audio update
    W_SIGNALS[2] = W_SIG_UPDATE_AUDIO;
    
    // Get audio buffer pointer
    audio_buffer = (uint8_t*)w_audio_ptr();
}

static void play_sound(const short* data, int size) {
    if (!audio_buffer) return;
    
    uint32_t w = W_SYS->audio_write;
    uint32_t audio_size = W_SYS->audio_size;
    
    for (int i = 0; i < size; i++) {
        // Convert 16-bit to 8-bit unsigned
        int16_t sample = data[i];
        uint8_t out = (uint8_t)((sample + 32768) >> 8);
        audio_buffer[w] = out;
        w = (w + 1) % audio_size;
    }
    W_SYS->audio_write = w;
}

// ============================================
// SETUP - Called once at start
// ============================================

void setup() {
    // Set window size
    wagno.width = 320;
    wagno.height = 240;
    wagno.scale = 4;
    
    // Initialize sprites
    player_sprite = wagno_create_image_from_data(
        player_sprite_data, 
        player_sprite_width, 
        player_sprite_height, 
        16  // RGB565
    );
    
    coin_sprite = wagno_create_image_from_data(
        coin_sprite_data,
        coin_sprite_width,
        coin_sprite_height,
        16  // RGB565
    );
    
    // Initialize audio
    init_audio();
    
    // Initialize player
    player.x = 100;
    player.y = GROUND_Y - player_sprite_height;
    player.vx = 0;
    player.vy = 0;
    player.width = player_sprite_width;
    player.height = player_sprite_height;
    player.on_ground = true;
    player.facing_right = true;
    
    // Create platforms
    platforms[num_platforms++] = (Platform){0, GROUND_Y, 1000, 40};       // Ground
    platforms[num_platforms++] = (Platform){100, 160, 80, 20};           // Platform 1
    platforms[num_platforms++] = (Platform){250, 130, 100, 20};          // Platform 2
    platforms[num_platforms++] = (Platform){400, 100, 80, 20};           // Platform 3
    platforms[num_platforms++] = (Platform){550, 140, 120, 20};          // Platform 4
    platforms[num_platforms++] = (Platform){700, 80, 80, 20};            // Platform 5
    platforms[num_platforms++] = (Platform){850, 120, 100, 20};          // Platform 6
    platforms[num_platforms++] = (Platform){0, 0, 20, GROUND_Y};         // Left wall
    platforms[num_platforms++] = (Platform){980, 0, 20, GROUND_Y};       // Right wall
    
    // Create coins
    coins[num_coins++] = (Coin){150, 140, false, 0.0f};
    coins[num_coins++] = (Coin){300, 110, false, 1.0f};
    coins[num_coins++] = (Coin){450, 80, false, 2.0f};
    coins[num_coins++] = (Coin){600, 120, false, 3.0f};
    coins[num_coins++] = (Coin){750, 60, false, 4.0f};
    coins[num_coins++] = (Coin){900, 100, false, 5.0f};
    
    // Set drawing defaults
    wagno_stroke(WAGNO_BLACK);
    wagno_stroke_weight(1);
}

// ============================================
// UPDATE - Called every frame (before draw)
// ============================================

void update() {
    // Handle mouse input
    if (wagno.mouse_down) {
        // Convert mouse screen position to world position
        float target_x = wagno.mouse.x + (float)camera_x;
        float dx = target_x - player.x;
        
        if (dx > 5) {
            player.vx = MOVE_SPEED;
            player.facing_right = true;
        } else if (dx < -5) {
            player.vx = -MOVE_SPEED;
            player.facing_right = false;
        } else {
            // Near target - slow down
            player.vx *= 0.7f;
        }
        
        // Jump if clicking above player
        if (player.on_ground && wagno.mouse.y < player.y - 10) {
            player.vy = JUMP_FORCE;
            player.on_ground = false;
        }
    } else {
        // Friction when not clicking
        player.vx *= 0.85f;
        if (player.vx * player.vx < 0.1f) player.vx = 0;
    }
    
    // Apply gravity
    player.vy += GRAVITY;
    
    // Clamp fall speed
    if (player.vy > 10.0f) player.vy = 10.0f;
    
    // === HORIZONTAL COLLISION ===
    float new_x = player.x + player.vx;
    
    for (int i = 0; i < num_platforms; i++) {
        Platform *p = &platforms[i];
        
        // Check if player overlaps platform horizontally with new position
        if (new_x + player.width > p->x && new_x < p->x + p->w &&
            player.y + player.height > p->y && player.y < p->y + p->h) {
            if (player.vx > 0) {
                // Moving right - push left
                new_x = (float)(p->x - player.width);
            } else if (player.vx < 0) {
                // Moving left - push right
                new_x = (float)(p->x + p->w);
            }
            player.vx = 0;
        }
    }
    player.x = new_x;
    
    // === VERTICAL COLLISION ===
    float new_y = player.y + player.vy;
    player.on_ground = false;
    
    for (int i = 0; i < num_platforms; i++) {
        Platform *p = &platforms[i];
        
        // Check if player overlaps platform vertically with new position
        if (player.x + player.width > p->x && player.x < p->x + p->w &&
            new_y + player.height > p->y && new_y < p->y + p->h) {
            if (player.vy > 0) {
                // Falling - land on top
                new_y = (float)(p->y - player.height);
                player.on_ground = true;
            } else if (player.vy < 0) {
                // Jumping up - hit bottom
                new_y = (float)(p->y + p->h);
            }
            player.vy = 0;
        }
    }
    player.y = new_y;
    
    // Keep player in world bounds
    if (player.x < 0) { player.x = 0; player.vx = 0; }
    if (player.x > world_width - player.width) { 
        player.x = (float)(world_width - player.width); 
        player.vx = 0; 
    }
    if (player.y < 0) { player.y = 0; player.vy = 0; }
    
    // Respawn if fell way below the world
    if (player.y > GROUND_Y + 100) {
        player.x = 100;
        player.y = GROUND_Y - player_sprite_height;
        player.vx = 0;
        player.vy = 0;
    }
    
    // Check coin collection
    for (int i = 0; i < num_coins; i++) {
        if (coins[i].collected) continue;
        
        float coin_x = coins[i].x;
        float coin_y = coins[i].y + (float)(wagno_sin(wagno.frame_count * 0.1f + coins[i].bob_offset) * 3);
        
        if (player.x + player.width > coin_x && player.x < coin_x + coin_sprite_width &&
            player.y + player.height > coin_y && player.y < coin_y + coin_sprite_height) {
            coins[i].collected = true;
            score++;
            play_sound(coin_sound_samples, COIN_SOUND_SIZE);
        }
    }
    
    // Update camera (smooth follow)
    int target_camera = (int)player.x - wagno.width / 2 + player.width / 2;
    if (target_camera < 0) target_camera = 0;
    if (target_camera > world_width - wagno.width) target_camera = world_width - wagno.width;
    camera_x = target_camera;
}

// ============================================
// DRAW - Called every frame
// ============================================

void draw() {
    // Draw sky background
    wagno_background(wagno_color_rgb(100, 150, 255));
    
    // Draw mountains (parallax)
    wagno_fill(wagno_color_rgb(80, 120, 80));
    wagno_no_stroke();
    for (int i = 0; i < 5; i++) {
        int mx = (i * 200 - camera_x / 3) % 1000;
        if (mx < -100) mx += 1000;
        wagno_triangle(mx, GROUND_Y, mx + 75, GROUND_Y - 80, mx + 150, GROUND_Y);
    }
    
    // Draw platforms
    for (int i = 0; i < num_platforms; i++) {
        int screen_x = platforms[i].x - camera_x;
        int screen_y = platforms[i].y;
        
        if (screen_x + platforms[i].w > 0 && screen_x < wagno.width) {
            // Platform body
            wagno_fill(wagno_color_rgb(100, 70, 50));
            wagno_no_stroke();
            wagno_rect(screen_x, screen_y, platforms[i].w, platforms[i].h);
            
            // Platform top (grass)
            if (platforms[i].h > 10) {
                wagno_fill(wagno_color_rgb(80, 160, 80));
                wagno_rect(screen_x, screen_y, platforms[i].w, 5);
            }
        }
    }
    
    // Draw coins (using sprite)
    for (int i = 0; i < num_coins; i++) {
        if (coins[i].collected) continue;
        
        int screen_x = (int)coins[i].x - camera_x;
        int screen_y = (int)coins[i].y + (int)(wagno_sin(wagno.frame_count * 0.1f + coins[i].bob_offset) * 3);
        
        if (screen_x + coin_sprite_width > 0 && screen_x < wagno.width) {
            wagno_image(coin_sprite, screen_x, screen_y);
        }
    }
    
    // Draw player (using sprite)
    int player_screen_x = (int)player.x - camera_x;
    int player_screen_y = (int)player.y;
    
    if (player.facing_right) {
        wagno_image(player_sprite, player_screen_x, player_screen_y);
    } else {
        // Flip sprite horizontally by drawing with negative width
        // Since we can't flip easily, just draw normally for now
        wagno_image(player_sprite, player_screen_x, player_screen_y);
    }
    
    // Draw HUD
    wagno_fill(WAGNO_BLACK);
    wagno_no_stroke();
    wagno_rect(5, 5, 60, 12);
    
    wagno_fill(WAGNO_WHITE);
    wagno_rect(6, 6, score * 8, 10);
    
    // Draw mouse cursor
    wagno_fill(WAGNO_YELLOW);
    wagno_no_stroke();
    wagno_rect(wagno.mouse.x - 2, wagno.mouse.y - 2, 5, 5);
    
    if (wagno.mouse_down) {
        wagno_fill(WAGNO_RED);
        wagno_rect(wagno.mouse.x - 4, wagno.mouse.y - 4, 9, 9);
    }
    
    // Draw win message
    if (score >= num_coins) {
        wagno_fill(WAGNO_BLACK);
        wagno_rect(wagno.width/2 - 60, wagno.height/2 - 20, 120, 40);
        
        wagno_fill(WAGNO_GREEN);
        wagno_rect(wagno.width/2 - 58, wagno.height/2 - 18, 116, 36);
        
        // Simple "WIN" text
        wagno_fill(WAGNO_WHITE);
        wagno_rect(wagno.width/2 - 40, wagno.height/2 - 10, 4, 20);
        wagno_rect(wagno.width/2 - 32, wagno.height/2 - 10, 4, 20);
        wagno_rect(wagno.width/2 - 36, wagno.height/2 + 5, 4, 5);
        
        wagno_rect(wagno.width/2 - 20, wagno.height/2 - 10, 4, 20);
        
        wagno_rect(wagno.width/2 - 10, wagno.height/2 - 10, 4, 20);
        wagno_rect(wagno.width/2 - 2, wagno.height/2 - 10, 4, 20);
        wagno_rect(wagno.width/2 + 2, wagno.height/2 - 5, 4, 5);
        wagno_rect(wagno.width/2 + 6, wagno.height/2, 4, 5);
    }
}

// ============================================
// CALLBACKS - Optional event handlers
// ============================================

void mouse_pressed() {}
void mouse_released() {}
void key_pressed(int key) {}
void key_released(int key) {}