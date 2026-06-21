#include "wagnostic.h"

// Benchmark Partículas: 50.000 partículas com física
// Estressa CPU (física) + VRAM (renderização)

#define WIDTH 1280
#define HEIGHT 720
#define NUM_PARTICLES 50000

typedef struct {
    float x, y;
    float vx, vy;
    float life;
    uint8_t r, g, b;
} Particle;

static Particle particles[NUM_PARTICLES];
static uint32_t frame_count = 0;

void winit() {
    w_setup("Particle Benchmark - 50K Particles", WIDTH, HEIGHT, 32, 1, 0);
    
    // Inicializar partículas
    for (int i = 0; i < NUM_PARTICLES; i++) {
        particles[i].x = WIDTH / 2.0f;
        particles[i].y = HEIGHT / 2.0f;
        particles[i].vx = ((float)(i % 1000) - 500.0f) * 0.01f;
        particles[i].vy = ((float)(i / 1000) - 50.0f) * 0.01f;
        particles[i].life = 1.0f;
        particles[i].r = (uint8_t)(i & 0xFF);
        particles[i].g = (uint8_t)((i * 7) & 0xFF);
        particles[i].b = (uint8_t)((i * 13) & 0xFF);
    }
}

void wupdate() {
    uint32_t* vram = (uint32_t*)W_FB_PTR;
    
    // Limpar framebuffer
    for (uint32_t i = 0; i < WIDTH * HEIGHT; i++) {
        vram[i] = W_RGBA(10, 10, 20, 255);
    }
    
    // Atualizar física das partículas
    float gravity = 0.1f;
    float damping = 0.99f;
    
    for (int i = 0; i < NUM_PARTICLES; i++) {
        // Gravidade
        particles[i].vy += gravity;
        
        // Velocidade
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        
        // Damping
        particles[i].vx *= damping;
        particles[i].vy *= damping;
        
        // Colisão com bordas
        if (particles[i].x < 0) {
            particles[i].x = 0;
            particles[i].vx = -particles[i].vx * 0.8f;
        }
        if (particles[i].x >= WIDTH) {
            particles[i].x = WIDTH - 1;
            particles[i].vx = -particles[i].vx * 0.8f;
        }
        if (particles[i].y < 0) {
            particles[i].y = 0;
            particles[i].vy = -particles[i].vy * 0.8f;
        }
        if (particles[i].y >= HEIGHT) {
            particles[i].y = HEIGHT - 1;
            particles[i].vy = -particles[i].vy * 0.8f;
        }
        
        // Vida
        particles[i].life -= 0.001f;
        if (particles[i].life <= 0) {
            // Respawn
            particles[i].x = WIDTH / 2.0f;
            particles[i].y = HEIGHT / 2.0f;
            particles[i].vx = ((float)((frame_count + i) % 100) - 50.0f) * 0.05f;
            particles[i].vy = ((float)((frame_count + i * 3) % 100) - 50.0f) * 0.05f;
            particles[i].life = 1.0f;
        }
        
        // Renderizar partícula (1 pixel)
        int px = (int)particles[i].x;
        int py = (int)particles[i].y;
        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
            uint8_t alpha = (uint8_t)(particles[i].life * 255.0f);
            vram[py * WIDTH + px] = W_RGBA(
                (uint8_t)(particles[i].r * particles[i].life),
                (uint8_t)(particles[i].g * particles[i].life),
                (uint8_t)(particles[i].b * particles[i].life),
                alpha
            );
        }
    }
    
    frame_count++;
    w_redraw();
}
