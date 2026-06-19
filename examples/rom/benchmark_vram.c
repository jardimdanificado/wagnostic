#include "wagnostic.h"

// Benchmark VRAM: Resolução altíssima (4K) com preenchimento de pixels
// Estressa bandwidth de memória e upload para o host

#define WIDTH 3840
#define HEIGHT 2160

static uint32_t frame_count = 0;

void winit() {
    w_setup("VRAM Benchmark - 4K Fill", WIDTH, HEIGHT, 32, 1, 0);
}

void wupdate() {
    uint32_t* vram = (uint32_t*)W_FB_PTR;
    uint32_t total_pixels = WIDTH * HEIGHT;
    
    // Padrão de gradiente animado que muda a cada frame
    uint32_t offset = frame_count * 256;
    
    for (uint32_t i = 0; i < total_pixels; i++) {
        uint32_t x = i % WIDTH;
        uint32_t y = i / WIDTH;
        
        // Gradiente diagonal animado
        uint8_t r = (uint8_t)((x + offset) & 0xFF);
        uint8_t g = (uint8_t)((y + offset) & 0xFF);
        uint8_t b = (uint8_t)((x + y + offset) & 0xFF);
        
        vram[i] = W_RGBA(r, g, b, 255);
    }
    
    frame_count++;
    w_redraw();
}
