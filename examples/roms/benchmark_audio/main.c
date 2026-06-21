#include "wagnostic.h"

// Benchmark Audio: 64 oscillators simultâneos, 48kHz, stereo, 32-bit float
// Estressa o audio ring buffer e processamento de áudio

#define WIDTH 320
#define HEIGHT 240
#define NUM_OSCILLATORS 64
#define SAMPLE_RATE 48000
#define AUDIO_BUFFER_SIZE (1024 * 1024)  // 1MB buffer

#define PI 3.14159265358979f
#define TWO_PI 6.28318530717959f

// Aproximação de seno por polinômio de Bhaskara I (sem libc)
static float fast_sin(float x) {
    // Normalizar para [0, 2*PI]
    while (x < 0) x += TWO_PI;
    while (x >= TWO_PI) x -= TWO_PI;
    // Aproximação parabólica: sin(x) ≈ 16x(PI-x) / (5*PI^2 - 4x(PI-x))
    if (x > PI) {
        float y = x - PI;
        float val = 16.0f * y * (PI - y) / (5.0f * PI * PI - 4.0f * y * (PI - y));
        return -val;
    }
    return 16.0f * x * (PI - x) / (5.0f * PI * PI - 4.0f * x * (PI - x));
}

typedef struct {
    float frequency;
    float phase;
    float amplitude;
    float type;  // 0=sine, 1=square, 2=saw, 3=triangle
} Oscillator;

static Oscillator oscillators[NUM_OSCILLATORS];
static uint32_t frame_count = 0;

void winit() {
    w_setup("Audio Benchmark - 64 Oscillators", WIDTH, HEIGHT, 32, 1, 0);
    
    // Configurar áudio
    W_SYS->audio_sample_rate = SAMPLE_RATE;
    W_SYS->audio_bpp = 4;  // 32-bit float
    W_SYS->audio_channels = 2;  // Stereo
    W_SYS->audio_size = AUDIO_BUFFER_SIZE;
    W_SYS->audio_write = 0;
    W_SYS->audio_read = 0;
    
    // Inicializar oscillators com frequências consonantes (escala musical)
    // Usando harmônicos de 220Hz (A3) em intervalos musicais
    float base_freq = 220.0f;
    float intervals[] = {1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f};  // Quinta, oitava, etc
    for (int i = 0; i < NUM_OSCILLATORS; i++) {
        oscillators[i].frequency = base_freq * intervals[i % 6] * (1 + i / 6);
        oscillators[i].phase = 0.0f;
        oscillators[i].amplitude = 0.05f / NUM_OSCILLATORS;  // Volume bem baixo (5%)
        oscillators[i].type = (float)(i % 2);  // Apenas sine e square suave
    }
}

void wupdate() {
    uint32_t* vram = (uint32_t*)W_FB_PTR;
    float* audio_buf = (float*)w_audio_ptr();
    uint32_t audio_size = W_SYS->audio_size;
    uint32_t write_ptr = W_SYS->audio_write;
    
    // Preencher VRAM com visualização simples
    for (uint32_t y = 0; y < HEIGHT; y++) {
        for (uint32_t x = 0; x < WIDTH; x++) {
            uint8_t intensity = (uint8_t)((x + y + frame_count) & 0xFF);
            vram[y * WIDTH + x] = W_RGBA(intensity, intensity / 2, 255 - intensity, 255);
        }
    }
    
    // Gerar áudio: 2048 samples stereo por frame (reduzido para evitar picos)
    uint32_t samples_to_write = 2048 * 2;  // Stereo
    
    // Envelope ADSR simples (fade in/out para evitar clicks)
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
        
        float left_sample = 0.0f;
        float right_sample = 0.0f;
        
        // Somar todos os oscillators
        for (int osc = 0; osc < NUM_OSCILLATORS; osc++) {
            float sample = 0.0f;
            float phase = oscillators[osc].phase;
            
            if (oscillators[osc].type < 0.5f) {
                // Sine
                sample = fast_sin(phase) * oscillators[osc].amplitude;
            } else {
                // Square suave (com slew rate para reduzir harshness)
                sample = (phase < PI ? 0.5f : -0.5f) * oscillators[osc].amplitude;
            }
            
            // Pan L/R baseado no índice do oscillator
            float pan = (float)osc / NUM_OSCILLATORS;
            left_sample += sample * (1.0f - pan);
            right_sample += sample * pan;
            
            // Avançar fase
            oscillators[osc].phase += TWO_PI * oscillators[osc].frequency / SAMPLE_RATE;
            if (oscillators[osc].phase >= TWO_PI) {
                oscillators[osc].phase -= TWO_PI;
            }
        }
        
        // Aplicar envelope e limitar volume máximo
        left_sample *= envelope * 0.3f;   // 30% do volume máximo
        right_sample *= envelope * 0.3f;
        
        // Soft clip para evitar distorção
        if (left_sample > 0.5f) left_sample = 0.5f;
        if (left_sample < -0.5f) left_sample = -0.5f;
        if (right_sample > 0.5f) right_sample = 0.5f;
        if (right_sample < -0.5f) right_sample = -0.5f;
        
        // Escrever no ring buffer
        audio_buf[write_ptr] = left_sample;
        write_ptr = (write_ptr + 1) % audio_size;
        audio_buf[write_ptr] = right_sample;
        write_ptr = (write_ptr + 1) % audio_size;
    }
    
    W_SYS->audio_write = write_ptr;
    frame_count++;
    w_redraw();
}
