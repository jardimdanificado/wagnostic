#define WAGNOSTIC_IMPLEMENTATION
#include "wagnostic.h"
#include "audio_data.h"

static uint16_t* _fb;
static uint8_t* _audio_buf;

void winit() {
    w_setup("Wagnostic - Audio Player", 320, 240, 16, 4, 0);
    _fb = (uint16_t*)w_vram;
    
    w_audio_size = music_size;
    w_audio_sample_rate = 44100;
    w_audio_bpp = 2;
    w_audio_channels = 2;
    w_audio_write = 0;
    w_audio_read = 0;

    _audio_buf = (uint8_t*)w_audio_buffer;
}

void fill_audio() {
    uint32_t r = w_audio_read;
    uint32_t w = w_audio_write;
    uint32_t size = w_audio_size;

    // Calculate occupied space
    uint32_t occupied;
    if (w >= r) occupied = w - r;
    else occupied = size - r + w;

    // We want to keep the buffer reasonably full, but not overflow
    // At 44.1kHz stereo 16-bit, we need ~176KB per second.
    // Let's keep about 0.5s of audio ahead (88KB)
    uint32_t target_buffer = 88200; 
    if (occupied >= target_buffer) return;

    uint32_t to_write = target_buffer - occupied;
    if (to_write > 16384) to_write = 16384; // write in chunks

    for (uint32_t i = 0; i < to_write; i++) {
        _audio_buf[w] = music_data[w];
        w = (w + 1) % size;
    }
    w_audio_write = w;
}

__attribute__((visibility("default")))
int wupdate() {
    static uint32_t last_tick = 0;
    static uint16_t color = 0;
    
    uint32_t now = w_ticks;
    if (now - last_tick > 1000) {
        color = (uint16_t)((w_audio_write >> 8) & 0xFFFF);
        last_tick = now;
    }

    for (int i = 0; i < 320 * 240; i++) {
        _fb[i] = color;
    }

    fill_audio();
    w_redraw(); return 1;
}
