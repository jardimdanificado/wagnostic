#include "wagnostic.h"

// Benchmark CPU: Mandelbrot fractal com muitas iterações
// Estressa cálculos de ponto flutuante no wasm3

#define WIDTH 512
#define HEIGHT 512
#define MAX_ITER 1000

static uint32_t frame_count = 0;

void winit() {
    w_setup("CPU Benchmark - Mandelbrot", WIDTH, HEIGHT, 32, 1, 0);
}

void wupdate() {
    uint32_t* vram = (uint32_t*)W_FB_PTR;
    uint32_t total_pixels = WIDTH * HEIGHT;
    
    // Mandelbrot com zoom animado
    float zoom = 1.0f + (frame_count % 1000) * 0.01f;
    float center_x = -0.5f;
    float center_y = 0.0f;
    
    for (uint32_t y = 0; y < HEIGHT; y++) {
        for (uint32_t x = 0; x < WIDTH; x++) {
            float real = (x - WIDTH / 2.0f) / (0.5f * zoom * WIDTH) + center_x;
            float imag = (y - HEIGHT / 2.0f) / (0.5f * zoom * HEIGHT) + center_y;
            
            float z_real = real;
            float z_imag = imag;
            
            uint32_t iter = 0;
            while (z_real * z_real + z_imag * z_imag < 4.0f && iter < MAX_ITER) {
                float z_real_new = z_real * z_real - z_imag * z_imag + real;
                z_imag = 2.0f * z_real * z_imag + imag;
                z_real = z_real_new;
                iter++;
            }
            
            // Colorir baseado em iterações
            uint8_t r, g, b;
            if (iter == MAX_ITER) {
                r = g = b = 0;
            } else {
                float t = (float)iter / MAX_ITER;
                r = (uint8_t)(9.0f * (1.0f - t) * t * t * t * 255.0f);
                g = (uint8_t)(15.0f * (1.0f - t) * (1.0f - t) * t * t * 255.0f);
                b = (uint8_t)(8.5f * (1.0f - t) * (1.0f - t) * (1.0f - t) * t * 255.0f);
            }
            
            vram[y * WIDTH + x] = W_RGBA(r, g, b, 255);
        }
    }
    
    frame_count++;
    w_redraw();
}
