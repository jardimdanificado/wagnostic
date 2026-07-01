#include <stdint.h>

void *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

typedef struct { int x, y, w, h; } Rect;

typedef struct {
    uint32_t width, height, bpp, scale;
    char title[128];
    uint32_t dirty_count;
    Rect dirty_rects[32];
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint8_t keys[256];
    uint32_t gamepad_buttons;
    uint32_t ticks;
    uint32_t target_fps;
    uint32_t audio_size, audio_sample_rate, audio_bpp, audio_channels;
    uint32_t audio_write, audio_read;
    uint32_t audio_underrun, audio_overrun;
    uint32_t vram_offset;
    uint32_t audio_buffer_offset;
    uint32_t io_load, io_load_buffer, io_load_size;
    uint32_t io_save, io_save_buffer, io_save_size;
    uint8_t reserved[16];
} State;

static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
    char loaded_text[128];
} rom;

static int state_phase = 0;
static const char *filename = "hello.txt";

int wupdate() {
    if (rom.s.width == 0) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.bpp = 16;
        rom.s.vram_offset = sizeof(State);
        rom.s.target_fps = 60;
        strcpy(rom.s.title, "Loading ZIP...");
    }

    if (state_phase == 0) {
        // Passo 1: Sondar o tamanho do arquivo
        rom.s.io_load = (uint32_t)filename;
        rom.s.io_load_buffer = 0;
        rom.s.io_load_size = 0;
        state_phase = 1;
    } else if (state_phase == 1) {
        // Host respondeu a sondagem preenchendo o tamanho e zerando io_load
        if (rom.s.io_load == 0 && rom.s.io_load_size > 0) {
            // Passo 2: Ler os dados de fato
            rom.s.io_load = (uint32_t)filename;
            rom.s.io_load_buffer = (uint32_t)rom.loaded_text;
            // O size já está correto, o Host vai usá-lo como capacidade.
            state_phase = 2;
        }
    } else if (state_phase == 2) {
        // Espera a leitura terminar (io_load volta pra zero)
        if (rom.s.io_load == 0) {
            // Leitura concluida
            rom.loaded_text[rom.s.io_load_size < 127 ? rom.s.io_load_size : 127] = '\0';
            strcpy(rom.s.title, rom.loaded_text);
            state_phase = 3;
        }
    } else if (state_phase == 3) {
        // Draw something
        uint16_t* vram = (uint16_t*)((uint8_t*)&rom.s + rom.s.vram_offset);
        for(int i=0; i<320*240; i++) vram[i] = 0x07E0; // Green screen
        rom.s.dirty_count = 1;
        rom.s.dirty_rects[0] = (Rect){0, 0, 320, 240};
        state_phase = 4;
    }

    return (int)&rom.s;
}
