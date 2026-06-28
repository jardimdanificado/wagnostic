// audio_wav — embedded WAV decoder (dr_wav) playing a sine tone

#include <stdint.h>

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#include "../audio_common/decoders/dr_wav.h"
#include "data/audio.h"

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
} State;

static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
    uint8_t audio_buffer[16384];
} rom;

static uint16_t* fb = (uint16_t*)rom.vram;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void set_pixel(int x, int y, uint16_t c) {
    if (x >= 0 && x < 320 && y >= 0 && y < 240) fb[y * 320 + x] = c;
}

static void fill_rect(int rx, int ry, int rw, int rh, uint16_t c) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++) set_pixel(x, y, c);
}

static const uint8_t font5x7[10][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
};

static void draw_digit(int x, int y, int d, uint16_t c) {
    if (d < 0 || d > 9) return;
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (font5x7[d][row] & (0x10 >> col)) set_pixel(x + col, y + row, c);
}

static void draw_number(int x, int y, int n, uint16_t c) {
    if (n < 0) { set_pixel(x, y + 3, c); n = -n; x += 7; }
    if (n == 0) { draw_digit(x, y, 0, c); return; }
    char buf[12]; int len = 0;
    while (n > 0 && len < 12) { buf[len++] = n % 10; n /= 10; }
    for (int i = len - 1; i >= 0; i--) { draw_digit(x, y, buf[i], c); x += 6; }
}

#define PCM_MAX_FRAMES (22050 * 2)
static int16_t  pcm[PCM_MAX_FRAMES * 2];
static uint32_t pcm_frames = 0;
static uint32_t play_pos   = 0;
static uint32_t src_rate   = 0;
static uint32_t src_ch     = 0;
static int      initialized = 0;

static void init_audio(void) {
    drwav wav;
    if (!drwav_init_memory(&wav, audio_bin, audio_bin_len, NULL)) return;
    src_rate = wav.sampleRate;
    src_ch   = wav.channels;
    uint64_t frames = wav.totalPCMFrameCount;
    if (frames > PCM_MAX_FRAMES) frames = PCM_MAX_FRAMES;
    drwav_read_pcm_frames_s16(&wav, frames, pcm);
    pcm_frames = (uint32_t)frames;
    drwav_uninit(&wav);
    rom.s.audio_sample_rate = src_rate;
    rom.s.audio_channels    = src_ch;
    rom.s.audio_bpp         = 2;
}

static void fill_audio(void) {
    if (!initialized || pcm_frames == 0) return;
    uint32_t r = rom.s.audio_read, w = rom.s.audio_write, sz = rom.s.audio_size;
    if (sz < 2) return;
    uint32_t used = (w >= r) ? (w - r) : (sz - r + w);
    uint32_t free_s = (sz - 1) - used;
    uint32_t to_write = free_s > 2048 ? 2048 : free_s;
    if (to_write == 0) return;

    uint32_t sample_bytes = src_ch * 2;
    to_write = (to_write / sample_bytes) * sample_bytes;
    if (to_write == 0) return;

    uint32_t i = 0;
    while (i < to_write) {
        uint32_t pos = (w + i) % sz;
        uint32_t to_end = sz - pos;
        if (to_end < sample_bytes) break;
        if (play_pos >= pcm_frames) play_pos = 0;
        const uint8_t* src = (const uint8_t*)&pcm[play_pos * src_ch];
        for (uint32_t b = 0; b < sample_bytes; b++)
            rom.audio_buffer[pos + b] = src[b];
        i += sample_bytes;
        play_pos++;
    }
    rom.s.audio_write = (w + i) % sz;
}

static void draw_ui(void) {
    fill_rect(0, 0, 320, 240, rgb565(15, 15, 20));

    fill_rect(10, 10, 130, 25, rgb565(30, 30, 40));
    fill_rect(15, 15, 10, 10, rgb565(0, 200, 255));
    fill_rect(30, 15, 10, 10, rgb565(80, 80, 80));
    fill_rect(45, 15, 10, 10, rgb565(80, 80, 80));
    fill_rect(60, 15, 10, 10, rgb565(80, 80, 80));
    draw_number(80, 15, (int)(pcm_frames / 1000), rgb565(180, 180, 180));

    int ox = 10, oy = 60, bar_w = 300, bar_h = 15;
    fill_rect(ox, oy, bar_w, bar_h, rgb565(20, 20, 30));
    int fill_w = pcm_frames ? (int)((long)bar_w * play_pos / pcm_frames) : 0;
    fill_rect(ox, oy, fill_w, bar_h, rgb565(0, 180, 100));
    draw_number(ox,         oy + bar_h + 5, (int)play_pos, rgb565(180, 180, 180));
    draw_number(ox + 100,   oy + bar_h + 5, (int)pcm_frames, rgb565(100, 100, 100));
    draw_number(ox + 200,   oy + bar_h + 5, (int)src_rate, rgb565(255, 200, 0));

    int woy = 110, h_h = 100;
    fill_rect(ox, woy, bar_w, h_h, rgb565(15, 15, 20));
    for (int x = 0; x < bar_w; x++) {
        uint32_t idx = pcm_frames ? (uint32_t)((long)x * pcm_frames / bar_w) : 0;
        int s = pcm[idx] / 256;
        int sy = woy + h_h/2 - s;
        if (sy >= woy && sy < woy + h_h) set_pixel(ox + x, sy, rgb565(0, 220, 150));
    }
}

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.bpp = 16;
        rom.s.scale = 2;
        rom.s.audio_size = 16384;
        rom.s.audio_sample_rate = 22050;
        rom.s.audio_bpp = 2;
        rom.s.audio_channels = 1;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        rom.s.audio_buffer_offset = (uint32_t)((uint8_t*)rom.audio_buffer - (uint8_t*)&rom.s);
        char* t = rom.s.title;
        const char* src = "Audio WAV (dr_wav)";
        int i = 0;
        while (src[i] && i < 127) { t[i] = src[i]; i++; }
        t[i] = '\0';
        init_audio();
        initialized = 1;
    }
    if (rom.s.keys[41]) return 0;
    static int sp_was = 0;
    if (rom.s.keys[44] && !sp_was) play_pos = 0;
    sp_was = rom.s.keys[44];

    fill_audio();
    draw_ui();

    rom.s.dirty_count = 1;
    rom.s.dirty_rects[0] = (Rect){0, 0, 320, 240};
    return (int)&rom.s;
}
