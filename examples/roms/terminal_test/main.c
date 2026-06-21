#include <stdint.h>
#include "wagnostic.h"

#define RED   0xE0
#define GREEN 0x1C
#define BLUE  0x03
#define WHITE 0xFF
#define BLACK 0x00
#define GRAY  0x6D // Aprox Gray em 8bpp

void draw_rect(int x, int y, int w, int h, uint8_t color) {
    uint8_t* vram = (uint8_t*)W_FB_PTR;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int cur_x = x + i;
            int cur_y = y + j;
            if (cur_x >= 0 && cur_x < 70 && cur_y >= 0 && cur_y < 24) {
                vram[cur_y * 70 + cur_x] = color;
            }
        }
    }
}

void winit() {
    W_SYS->width = 70;
    W_SYS->height = 24;
    W_SYS->bpp = 8;
    W_SYS->scale = 1;
}

void wupdate() {
    uint8_t* vram = (uint8_t*)W_FB_PTR;
    
    // 1. Limpa Fundo
    for (int i = 0; i < 70 * 24; i++) vram[i] = BLACK;

    // 2. Desenha um "Teclado Visual" Simplificado (A-Z)
    for (int i = 0; i < 26; i++) {
        int x = 2 + (i % 13) * 5;
        int y = 5 + (i / 13) * 4;
        
        // Se a tecla estiver pressionada (HID Scancodes 4 a 29 para A-Z)
        uint8_t color = (W_SYS->keys[4 + i]) ? GREEN : GRAY;
        draw_rect(x, y, 4, 3, color);
    }

    // 3. Teclas Especiais
    if (W_SYS->keys[41]) draw_rect(0, 0, 5, 2, RED);      // ESC (Canto superior esquerdo)
    if (W_SYS->keys[40]) draw_rect(65, 0, 5, 2, BLUE);     // Enter (Canto superior direito)
    if (W_SYS->keys[42]) draw_rect(55, 0, 5, 2, 0xFC);     // Backspace (Amarelo/Laranja)
    if (W_SYS->keys[44]) draw_rect(20, 20, 30, 2, WHITE);  // Space (Barra inferior)

    // 4. Mouse Tracker
    int mx = W_SYS->mouse_x;
    int my = W_SYS->mouse_y;
    
    // Desenha linhas de guia do mouse (Crosshair)
    uint8_t guide_color = (W_SYS->mouse_buttons & 1) ? RED : 0x24; // Vermelho se clicar, senão cinza escuro
    for (int i = 0; i < 70; i++) vram[my * 70 + i] = guide_color;
    for (int i = 0; i < 24; i++) vram[i * 70 + mx] = guide_color;
    
    // Pixel central do mouse
    if (mx >= 0 && mx < 70 && my >= 0 && my < 24) {
        vram[my * 70 + mx] = WHITE;
    }

    W_SIGNALS[0] = W_SIG_REDRAW;
}
