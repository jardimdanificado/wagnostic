#include "wagnostic.h"

#define SAMPLE_RATE 44100

static uint16_t* _fb;
static int16_t* _audio_buf;
static float phase = 0;
static uint32_t frame_count = 0;

__attribute__((visibility("default")))
void winit() {
    w_setup("Wagnostic - Audio Test", 320, 240, 16, 2, 8);
    _fb = (uint16_t*)W_FB_PTR;
    
    W_SYS->audio_size = 16384;
    W_SYS->audio_sample_rate = SAMPLE_RATE;
    W_SYS->audio_bpp = 2;
    W_SYS->audio_channels = 1;
    
    W_SIGNALS[2] = W_SIG_UPDATE_AUDIO; 
    _audio_buf = (int16_t*)w_audio_ptr();
}

__attribute__((visibility("default")))
void wupdate() {
    uint32_t write_ptr = W_SYS->audio_write;
    uint32_t read_ptr = W_SYS->audio_read;
    uint32_t size = W_SYS->audio_size;

    uint32_t free_space;
    if (write_ptr >= read_ptr) {
        free_space = size - (write_ptr - read_ptr);
    } else {
        free_space = read_ptr - write_ptr;
    }

    uint32_t to_write = free_space > 1024 ? 1024 : free_space;
    for (uint32_t i = 0; i < to_write / 2; i++) {
        // Lower volume: multiplier reduced to 4096.0f
        int16_t sample = (int16_t)((phase * 2.0f - 1.0f) * 4096.0f);
        _audio_buf[(write_ptr / 2 + i) % (size / 2)] = sample;
        
        phase += 440.0f / SAMPLE_RATE;
        if (phase > 1.0f) phase -= 1.0f;
    }
    W_SYS->audio_write = (write_ptr + to_write) % size;

    // Slow visual feedback: color changes every 30 frames (~0.5s)
    frame_count++;
    uint16_t color = (frame_count / 30) % 2 ? 0x001F : 0x000F; // Deep Blue to Mid Blue
    for(int i=0; i<320*240; i++) _fb[i] = color;
    
    w_redraw();
}
