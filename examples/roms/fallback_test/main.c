#define WAGNOSTIC_IMPLEMENTATION
#include "wagnostic.h"


// Sem winit! O Host deve aplicar os defaults: 320x240, 8bpp, scale 1.
__attribute__((visibility("default")))
int wupdate() {
    uint8_t* fb = (uint8_t*)w_vram;
    static uint32_t last_tick = 0;
    static uint8_t color = 0;
    
    uint32_t now = w_ticks;
    if (now - last_tick > 1000) {
        color += 32; // Muda a cor levemente a cada 1 segundo
        last_tick = now;
    }
    
    // Preenche a tela com a cor estática
    for (int i = 0; i < 320 * 240; i++) {
        fb[i] = color;
    }
    
    w_redraw(); return 1;
}
