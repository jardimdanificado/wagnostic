#include "wagnostic.h"

// Benchmark ALL: Combina tudo - alta resolução + partículas + áudio + cálculos
// O stress test definitivo

#define WIDTH 1280
#define HEIGHT 720
#define NUM_PARTICLES 5000
#define NUM_OSCILLATORS 16
#define SAMPLE_RATE 48000
#define AUDIO_BUFFER_SIZE (1024 * 1024)  // 1MB buffer
#define MANDEL_ITER 32

#define PI 3.14159265358979f
#define TWO_PI 6.28318530717959f

// Aproximação de seno (Bhaskara I) - sem libc
static float fast_sin(float x) {
    while (x < 0) x += TWO_PI;
    while (x >= TWO_PI) x -= TWO_PI;
    if (x > PI) {
        float y = x - PI;
        float val = 16.0f * y * (PI - y) / (5.0f * PI * PI - 4.0f * y * (PI - y));
        return -val;
    }
    return 16.0f * x * (PI - x) / (5.0f * PI * PI - 4.0f * x * (PI - x));
}

typedef struct {
    float x, y;
    float vx, vy;
    float life;
    uint8_t r, g, b;
} Particle;

typedef struct {
    float frequency;
    float phase;
    float amplitude;
} Oscillator;

static Particle particles[NUM_PARTICLES];
static Oscillator oscillators[NUM_OSCILLATORS];
static uint32_t frame_count = 0;

void winit() {
    w_setup("STRESS TEST - All Systems", WIDTH, HEIGHT, 32, 1, 0);
    
    // Configurar áudio
    W_SYS->audio_sample_rate = SAMPLE_RATE;
    W_SYS->audio_bpp = 4;
    W_SYS->audio_channels = 2;
    W_SYS->audio_size = AUDIO_BUFFER_SIZE;
    
    // Inicializar partículas
    for (int i = 0; i < NUM_PARTICLES; i++) {
        particles[i].x = WIDTH / 2.0f;
        particles[i].y = HEIGHT / 2.0f;
        particles[i].vx = ((float)(i % 100) - 50.0f) * 0.1f;
        particles[i].vy = ((float)(i / 100) - 100.0f) * 0.1f;
        particles[i].life = 1.0f;
        particles[i].r = (uint8_t)(i & 0xFF);
        particles[i].g = (uint8_t)((i * 3) & 0xFF);
        particles[i].b = (uint8_t)((i * 7) & 0xFF);
    }
    
    // Inicializar oscillators com frequências consonantes (harmônicos musicais)
    float base_freq = 220.0f;
    float intervals[] = {1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f};
    for (int i = 0; i < NUM_OSCILLATORS; i++) {
        oscillators[i].frequency = base_freq * intervals[i % 6] * (1 + i / 6);
        oscillators[i].phase = 0.0f;
        oscillators[i].amplitude = 0.03f / NUM_OSCILLATORS;  // Volume bem baixo (3%)
    }
}

void wupdate() {
    uint32_t* vram = (uint32_t*)W_FB_PTR;
    float* audio_buf = (float*)w_audio_ptr();
    uint32_t audio_size = W_SYS->audio_size;
    uint32_t write_ptr = W_SYS->audio_write;
    
    // 1. Preencher VRAM com gradiente animado (simples mas estressa bandwidth)
    uint32_t offset = frame_count * 256;
    for (uint32_t y = 0; y < HEIGHT; y++) {
        for (uint32_t x = 0; x < WIDTH; x++) {
            uint8_t r = (uint8_t)((x + offset) & 0xFF);
            uint8_t g = (uint8_t)((y + offset) & 0xFF);
            uint8_t b = (uint8_t)((x + y + offset) & 0xFF);
            vram[y * WIDTH + x] = W_RGBA(r, g, b, 255);
        }
    }
    
    // 2. Atualizar e renderizar partículas
    for (int i = 0; i < NUM_PARTICLES; i++) {
        particles[i].vy += 0.05f;
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].vx *= 0.99f;
        particles[i].vy *= 0.99f;
        
        if (particles[i].x < 0 || particles[i].x >= WIDTH || 
            particles[i].y < 0 || particles[i].y >= HEIGHT) {
            particles[i].x = WIDTH / 2.0f;
            particles[i].y = HEIGHT / 2.0f;
            particles[i].vx = ((float)((frame_count + i) % 100) - 50.0f) * 0.1f;
            particles[i].vy = ((float)((frame_count + i * 2) % 100) - 50.0f) * 0.1f;
            particles[i].life = 1.0f;
        }
        
        particles[i].life -= 0.002f;
        if (particles[i].life <= 0) {
            particles[i].life = 1.0f;
        }
        
        int px = (int)particles[i].x;
        int py = (int)particles[i].y;
        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
            vram[py * WIDTH + px] = W_RGBA(
                (uint8_t)(particles[i].r * particles[i].life),
                (uint8_t)(particles[i].g * particles[i].life),
                (uint8_t)(particles[i].b * particles[i].life),
                255
            );
        }
    }
    
    // 3. Gerar áudio com envelope e volume controlado
    uint32_t samples_to_write = 1024 * 2;
    
    // Envelope ADSR simples
    float envelope = 1.0f;
    uint32_t attack_samples = 100;
    uint32_t release_samples = 100;
    
    for (uint32_t i = 0; i < samples_to_write; i += 2) {
        // Aplicar envelope
        if (i < attack_samples) {
            envelope = (float)i / attack_samples;
        } else if (i > samples_to_write - release_samples) {
            envelope = (float)(samples_to_write - i) / release_samples;
        } else {
            envelope = 1.0f;
        }
        
        float left = 0.0f;
        float right = 0.0f;
        
        for (int osc = 0; osc < NUM_OSCILLATORS; osc++) {
            float sample = fast_sin(oscillators[osc].phase) * oscillators[osc].amplitude;
            float pan = (float)osc / NUM_OSCILLATORS;
            left += sample * (1.0f - pan);
            right += sample * pan;
            
            oscillators[osc].phase += TWO_PI * oscillators[osc].frequency / SAMPLE_RATE;
            if (oscillators[osc].phase >= TWO_PI) {
                oscillators[osc].phase -= TWO_PI;
            }
        }
        
        // Aplicar envelope e limitar volume (20% do máximo)
        left *= envelope * 0.2f;
        right *= envelope * 0.2f;
        
        // Soft clip para evitar distorção
        if (left > 0.4f) left = 0.4f;
        if (left < -0.4f) left = -0.4f;
        if (right > 0.4f) right = 0.4f;
        if (right < -0.4f) right = -0.4f;
        
        audio_buf[write_ptr] = left;
        write_ptr = (write_ptr + 1) % audio_size;
        audio_buf[write_ptr] = right;
        write_ptr = (write_ptr + 1) % audio_size;
    }
    
    W_SYS->audio_write = write_ptr;
    frame_count++;
    w_redraw();
}
