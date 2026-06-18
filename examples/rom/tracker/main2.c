
// =============================================================================
//  Wagnostic Music Tracker Example
//  A simple 4-channel chiptune tracker ROM
//  Controls:
//    Arrow keys  - navigate pattern
//    Space       - play/stop
//    Tab         - next pattern
//    Shift+Tab   - prev pattern
//    F1-F4       - select instrument
//    A-L row     - enter notes (piano-style: A=C, W=C#, S=D, E=D#, ...)
//    Delete      - clear cell
//    Z           - note off (---) on current cell
//    Escape       - stop
// =============================================================================

#define sinf __builtin_sinf
#define fabsf __builtin_fabsf

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;
typedef          short int16_t;


extern uint32_t get_ticks();

#pragma pack(push, 1)
typedef struct {
    char     message[128];
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t scale;
    uint32_t audio_size;
    uint32_t audio_write;
    uint32_t audio_read;
    uint32_t audio_sample_rate, audio_bpp, audio_channels;
    uint32_t signal_count;
    uint32_t gamepad_buttons;
    int32_t  joystick_lx, joystick_ly, joystick_rx, joystick_ry;
    uint8_t  keys[256];
    int32_t  mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t  mouse_wheel;
    uint8_t  reserved[48];
} SystemConfig;
#pragma pack(pop)

#define _sys   ((volatile SystemConfig*)0)


static uint16_t* _fb;
#define _sig   ((volatile uint8_t*)512)

// ---- Scancodes (SDL2 / USB HID) ----
#define KEY_A       4
#define KEY_B       5
#define KEY_C       6
#define KEY_D       7
#define KEY_E       8
#define KEY_F       9
#define KEY_G       10
#define KEY_H       11
#define KEY_I       12
#define KEY_J       13
#define KEY_K       14
#define KEY_L       15
#define KEY_M       16
#define KEY_N       17
#define KEY_O       18
#define KEY_P       19
#define KEY_Q       20
#define KEY_R       21
#define KEY_S       22
#define KEY_T       23
#define KEY_U       24
#define KEY_V       25
#define KEY_W       26
#define KEY_X       27
#define KEY_Y       28
#define KEY_Z       29
#define KEY_1       30
#define KEY_2       31
#define KEY_3       32
#define KEY_4       33
#define KEY_5       34
#define KEY_6       35
#define KEY_7       36
#define KEY_8       37
#define KEY_9       38
#define KEY_0       39
#define KEY_ENTER   40
#define KEY_ESCAPE  41
#define KEY_BACKSP  42
#define KEY_TAB     43
#define KEY_SPACE   44
#define KEY_DELETE  76
#define KEY_RIGHT   79
#define KEY_LEFT    80
#define KEY_DOWN    81
#define KEY_UP      82
#define KEY_LSHIFT  225
#define KEY_RSHIFT  229
#define KEY_F1      58
#define KEY_F2      59
#define KEY_F3      60
#define KEY_F4      61

// ---- Audio ----
#define SAMPLE_RATE  44100
#define AUDIO_SIZE   32768
#define NUM_CH       4
#define PI           3.14159265359f
#define TWO_PI       6.28318530718f

// ---- Tracker constants ----
#define ROWS_PER_PAT  32
#define NUM_PATTERNS  8
#define NUM_INSTRS    4
#define SCREEN_W      320
#define SCREEN_H      240

// ---- Note encoding ----
// 0   = empty (----)
// 255 = note off
// 1..96 = notes C1..B8 (96 = 8 octaves * 12 notes)
#define NOTE_EMPTY  0
#define NOTE_OFF    255

// ---- Instrument types ----
#define INST_SQUARE   0
#define INST_TRIANGLE 1
#define INST_SAW      2
#define INST_NOISE    3

// ---- Note frequencies (C1 = note 1) ----
// freq = 32.703 * 2^((note-1)/12)
static float note_freq(int note) {
    if (note <= 0 || note == NOTE_OFF) return 0.0f;
    // note 1 = C1 = 32.703 Hz
    float f = 32.703f;
    int n = note - 1;
    for (int i = 0; i < n; i++) f *= 1.05946309436f; // 2^(1/12)
    return f;
}

static const char* NOTE_NAMES[] = {
    "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
};

// ---- Pattern cell ----
typedef struct {
    uint8_t note;    // 0=empty, 255=off, 1-96=note
    uint8_t inst;    // instrument (0-3)
    uint8_t vol;     // volume 0-64 (64=full)
} Cell;

// ---- Song data ----
static Cell patterns[NUM_PATTERNS][ROWS_PER_PAT][NUM_CH];
static uint8_t song_order[64];   // which pattern index plays at each song position
static int song_length = 4;

// ---- Playback state ----
typedef struct {
    float phase;
    float freq;
    int   inst;
    int   vol;        // 0-64
    int   active;
    uint32_t noise_state; // LFSR for noise
    // Envelope
    int   env_pos;    // sample counter since note start
    int   env_att;    // attack in samples
    int   env_rel;    // release in samples (0=no release yet)
    int   releasing;
} Channel;

static Channel channels[NUM_CH];

// ---- Sequencer state ----
static int  playing       = 0;
static int  cur_row       = 0;
static int  cur_col       = 0;
static int  cur_pat       = 0;
static int  cur_inst      = 0;
static int  play_song_pos = 0;
static int  play_pat      = 0;
static int  play_row      = 0;
static int  bpm           = 125;
static int  ticks_per_row = 6;  // speed (like MOD format)

// samples per row = (SAMPLE_RATE * 60) / (bpm * ticks_per_row * (1/4))
// simplified: samples_per_row = (SAMPLE_RATE * 60 * 4) / (bpm * ticks_per_row * 4)
//           = SAMPLE_RATE * 60 / (bpm * ticks_per_row)
// standard tracker: 24 ticks/beat, bpm beats/min
// samples_per_row = SAMPLE_RATE * 60 / (bpm * 24 / ticks_per_row)
// Let's just use: samples_per_row = SAMPLE_RATE * 2.5 / bpm  (common approximation)

static int samples_per_row;
static int row_sample_count = 0;

// Audio ring buffer
static uint8_t* audio_buf_ptr;

// ---- Key state ----
static uint8_t prev_keys[256];
static uint8_t key_just_pressed[256];

// ---- Simple font (5x7 bitmap, ASCII 32-127) ----
// We'll use a minimal 4x6 bitmap font embedded as bytes

// Each char: 5 columns, 7 rows, packed as 5 bytes (each byte = 7 bits = 7 rows)
static const uint8_t font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x04,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x7F,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x10,0x08,0x08,0x10,0x08}, // '~'
};

// ---- RGB565 helpers ----
#define RGB(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))

// Color palette
#define COL_BG         RGB(10,  10,  15)
#define COL_HEADER_BG  RGB(20,  20,  40)
#define COL_GRID_BG    RGB(14,  14,  25)
#define COL_GRID_ALT   RGB(18,  18,  32)
#define COL_CURSOR     RGB(60, 180, 255)
#define COL_PLAY_ROW   RGB(40, 100, 40)
#define COL_NOTE_C     RGB(255, 220, 100)
#define COL_NOTE       RGB(180, 220, 255)
#define COL_NOTE_OFF   RGB(100, 100, 160)
#define COL_INST_0     RGB(100, 255, 180)
#define COL_INST_1     RGB(255, 150, 80)
#define COL_INST_2     RGB(200, 100, 255)
#define COL_INST_3     RGB(100, 200, 255)
#define COL_VOL        RGB(160, 200, 120)
#define COL_TEXT       RGB(200, 200, 220)
#define COL_DIM        RGB(80,  80,  100)
#define COL_VU_BG      RGB(20,  40,  20)
#define COL_VU_LOW     RGB(80,  220, 80)
#define COL_VU_MID     RGB(220, 220, 40)
#define COL_VU_HIGH    RGB(255, 60,  60)
#define COL_BORDER     RGB(50,  60,  100)
#define COL_PLAYING    RGB(80,  255, 120)
#define COL_STOPPED    RGB(255, 80,  80)

// ---- Drawing ----
static void put_pixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    _fb[y * SCREEN_W + x] = color;
}

static void fill_rect(int x, int y, int w, int h, uint16_t color) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            put_pixel(x+i, y+j, color);
}

static void draw_hline(int x, int y, int w, uint16_t color) {
    for (int i = 0; i < w; i++) put_pixel(x+i, y, color);
}

static void draw_char(int x, int y, char c, uint16_t fg) {
    if (c < 32 || c > 126) return;
    const uint8_t* glyph = font5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (bits & (1 << row))
                put_pixel(x + col, y + row, fg);
        }
    }
}

static void draw_str(int x, int y, const char* s, uint16_t fg) {
    while (*s) {
        draw_char(x, y, *s, fg);
        x += 6;
        s++;
    }
}

static void draw_str_bg(int x, int y, const char* s, uint16_t fg, uint16_t bg) {
    int len = 0;
    while (s[len]) len++;
    fill_rect(x, y, len*6, 8, bg);
    draw_str(x, y, s, fg);
}

// ---- Utility: int to string (no libc) ----
static char* itoa_buf(int v, char* buf, int base) {
    if (v == 0) { buf[0]='0'; buf[1]=0; return buf; }
    char tmp[16]; int i=0;
    int neg = 0;
    if (v < 0) { neg=1; v=-v; }
    while (v > 0) { tmp[i++] = "0123456789ABCDEF"[v%base]; v/=base; }
    if (neg) tmp[i++]='-';
    int j=0;
    for (int k=i-1; k>=0; k--) buf[j++]=tmp[k];
    buf[j]=0;
    return buf;
}

static void draw_int(int x, int y, int v, uint16_t fg) {
    char buf[16];
    itoa_buf(v, buf, 10);
    draw_str(x, y, buf, fg);
}

// ---- Note name helpers ----
static void note_to_str(uint8_t note, char* out) {
    if (note == NOTE_EMPTY) { out[0]='-'; out[1]='-'; out[2]='-'; out[3]=0; return; }
    if (note == NOTE_OFF)   { out[0]='='; out[1]='='; out[2]='='; out[3]=0; return; }
    int n = (note-1) % 12;
    int oct = (note-1) / 12 + 1;
    out[0] = NOTE_NAMES[n][0];
    out[1] = NOTE_NAMES[n][1];
    out[2] = '0' + oct;
    out[3] = 0;
}

// ---- Hex digit ----
static char hex1(int v) { return "0123456789ABCDEF"[v & 0xF]; }

// ---- Audio synthesis ----
static float synth_sample(Channel* ch) {
    if (!ch->active || ch->freq <= 0.0f) return 0.0f;

    float t = ch->phase / TWO_PI;
    float s = 0.0f;

    switch (ch->inst) {
        case INST_SQUARE:
            s = (t < 0.5f) ? 1.0f : -1.0f;
            break;
        case INST_TRIANGLE:
            s = (t < 0.5f) ? (4.0f*t - 1.0f) : (3.0f - 4.0f*t);
            break;
        case INST_SAW:
            s = 2.0f*t - 1.0f;
            break;
        case INST_NOISE: {
            // LFSR noise, update every 16 samples for pitched feel
            ch->noise_state = ch->noise_state * 1664525u + 1013904223u;
            s = ((int)(ch->noise_state >> 16) & 1) ? 1.0f : -1.0f;
            break;
        }
    }

    // Envelope: attack 10ms, release 20ms
    float env = 1.0f;
    int att_samples = (SAMPLE_RATE * 10) / 1000;
    if (ch->env_pos < att_samples) {
        env = (float)ch->env_pos / att_samples;
    }
    if (ch->releasing) {
        int rel_samples = (SAMPLE_RATE * 40) / 1000;
        float t_rel = (float)ch->env_rel / rel_samples;
        if (t_rel >= 1.0f) { ch->active = 0; return 0.0f; }
        env *= (1.0f - t_rel);
        ch->env_rel++;
    }
    ch->env_pos++;

    s *= env * ((float)ch->vol / 64.0f) * 0.22f;

    ch->phase += TWO_PI * ch->freq / SAMPLE_RATE;
    if (ch->phase > TWO_PI) ch->phase -= TWO_PI;

    return s;
}

// VU meter values per channel (0-64)
static int vu_meter[NUM_CH];
static int vu_peak[NUM_CH];
static int vu_peak_hold[NUM_CH]; // frames to hold peak

// ---- Trigger a note on a channel ----
static void trigger_note(int ch_idx, uint8_t note, uint8_t inst, uint8_t vol) {
    Channel* ch = &channels[ch_idx];
    if (note == NOTE_OFF) {
        ch->releasing = 1;
        ch->env_rel = 0;
        return;
    }
    if (note == NOTE_EMPTY) return;
    ch->freq = note_freq(note);
    ch->inst = inst;
    ch->vol = vol > 64 ? 64 : vol;
    ch->active = 1;
    ch->releasing = 0;
    ch->env_pos = 0;
    ch->env_rel = 0;
    ch->phase = 0.0f;
    ch->noise_state = 0xDEADBEEFu;
}

// ---- Advance sequencer one row ----
static void seq_advance_row() {
    play_row++;
    if (play_row >= ROWS_PER_PAT) {
        play_row = 0;
        play_song_pos++;
        if (play_song_pos >= song_length) play_song_pos = 0;
        play_pat = song_order[play_song_pos];
    }
}

// ---- Fill audio buffer ----
static int audio_gen_sample_pos = 0; // tracks fractional sample position within row

static void fill_audio() {
    uint32_t r = _sys->audio_read;
    uint32_t w = _sys->audio_write;
    uint32_t size = _sys->audio_size;

    int avail_bytes = (r > w) ? (int)(r - w - 2) : (int)(size - w + r - 2);
    if (avail_bytes < 4) return;

    int max_samples = avail_bytes / 4;
    if (max_samples > 256) max_samples = 256;

    for (int i = 0; i < max_samples; i++) {
        // Advance sequencer when row completes
        if (audio_gen_sample_pos >= samples_per_row) {
            audio_gen_sample_pos = 0;
            if (playing) {
                // Trigger notes for this row
                for (int c = 0; c < NUM_CH; c++) {
                    Cell* cell = &patterns[play_pat][play_row][c];
                    if (cell->note != NOTE_EMPTY || cell->vol != 0) {
                        trigger_note(c, cell->note, cell->inst, cell->vol);
                    }
                }
                seq_advance_row();
            }
        }
        audio_gen_sample_pos++;

        float mix = 0.0f;
        for (int c = 0; c < NUM_CH; c++) {
            float s = synth_sample(&channels[c]);
            mix += s;
            // VU
            int v = (int)(fabsf(s) * 64.0f * 4.5f);
            if (v > 64) v = 64;
            if (v > vu_meter[c]) vu_meter[c] = v;
        }
        if (mix >  1.0f) mix =  1.0f;
        if (mix < -1.0f) mix = -1.0f;

        int16_t samp = (int16_t)(mix * 32000.0f);
        uint32_t pos = w;
        int16_t* out = (int16_t*)(audio_buf_ptr + pos);
        out[0] = samp;
        out[1] = samp;
        w = (w + 4) % size;
    }
    _sys->audio_write = w;
}

// ---- UI layout ----
// Top bar: 0..11 (h=12)
// Column headers: 12..19 (h=8)
// Pattern grid: 20..179 (h=160, 32 rows * 5px each)
// Status bar: 232..239 (h=8)
// VU meters: 180..229 (h=50)

#define TOP_BAR_Y      0
#define TOP_BAR_H      13
#define COL_HEADER_Y   13
#define COL_HEADER_H   8
#define GRID_TOP_Y     21
#define ROW_H          5
#define GRID_ROWS_VISIBLE 32
#define VU_Y           (GRID_TOP_Y + GRID_ROWS_VISIBLE * ROW_H + 2)
#define VU_H           32
#define STATUS_Y       (SCREEN_H - 9)

// Column x positions for the 4 tracker channels
// Each channel: row-num(3ch) + note(4ch) + inst(2ch) + vol(2ch) + sep = ~12 chars wide
// Total chars: 4 + 3 * 4 channels ~ too wide at 6px per char = 4*13*6 = 312px
// Let's calculate precisely:
// row: "00 " = 18px
// ch: "C-4 0 40 " = note(4*6=24) + inst(1*6=6) + vol(2*6=12) + space(6) = 48px per ch
// 4 channels: 4*48 = 192px, row = 18, total = 210... fits in 320

#define ROW_NUM_X   1
#define CH_WIDTH    56  // pixels per channel column
#define CH0_X       24
#define CH1_X       (CH0_X + CH_WIDTH)
#define CH2_X       (CH1_X + CH_WIDTH)
#define CH3_X       (CH2_X + CH_WIDTH)

static int ch_x[4] = { CH0_X, CH1_X, CH2_X, CH3_X };

// ---- Draw VU bar for one channel ----
static void draw_vu(int ch_idx) {
    int x = CH0_X + ch_idx * CH_WIDTH + CH_WIDTH/2 - 5;
    int y = VU_Y;
    int w = 10;
    int h = VU_H;

    fill_rect(x, y, w, h, COL_VU_BG);

    int filled = (vu_meter[ch_idx] * h) / 64;
    for (int j = 0; j < filled; j++) {
        uint16_t col;
        int pos = j * 64 / h;
        if (pos < 40) col = COL_VU_LOW;
        else if (pos < 55) col = COL_VU_MID;
        else col = COL_VU_HIGH;
        for (int i = 0; i < w; i++)
            put_pixel(x+i, y + h - 1 - j, col);
    }

    // Peak indicator
    if (vu_peak[ch_idx] > 0) {
        int peak_y = y + h - 1 - (vu_peak[ch_idx] * h / 64);
        draw_hline(x, peak_y, w, COL_VU_MID);
    }
}

// ---- Draw everything ----
static void draw_frame() {
    // Background
    fill_rect(0, 0, SCREEN_W, SCREEN_H, COL_BG);

    // Top bar
    fill_rect(0, TOP_BAR_Y, SCREEN_W, TOP_BAR_H, COL_HEADER_BG);
    draw_hline(0, TOP_BAR_H - 1, SCREEN_W, COL_BORDER);

    // Title
    draw_str(2, 3, "WAGNOSTIC TRACKER", COL_TEXT);

    // BPM
    char buf[32];
    draw_str(150, 3, "BPM:", COL_DIM);
    itoa_buf(bpm, buf, 10);
    draw_str(174, 3, buf, COL_NOTE_C);

    // Speed
    draw_str(210, 3, "SPD:", COL_DIM);
    itoa_buf(ticks_per_row, buf, 10);
    draw_str(234, 3, buf, COL_NOTE_C);

    // Pattern / song pos
    draw_str(258, 3, "PAT:", COL_DIM);
    itoa_buf(cur_pat, buf, 10);
    draw_str(282, 3, buf, COL_NOTE_C);

    // Play status indicator
    uint16_t play_col = playing ? COL_PLAYING : COL_STOPPED;
    fill_rect(308, 4, 8, 6, play_col);

    // Column headers
    fill_rect(0, COL_HEADER_Y, SCREEN_W, COL_HEADER_H, COL_HEADER_BG);
    draw_hline(0, COL_HEADER_Y + COL_HEADER_H - 1, SCREEN_W, COL_BORDER);

    static const char* ch_names[] = { "CH1", "CH2", "CH3", "CH4" };
    static const uint16_t ch_cols[] = { COL_INST_0, COL_INST_1, COL_INST_2, COL_INST_3 };
    for (int c = 0; c < NUM_CH; c++) {
        int x = ch_x[c];
        draw_str(x, COL_HEADER_Y + 1, ch_names[c], ch_cols[c]);
        // Separator
        if (c < NUM_CH-1)
            for (int j = 0; j < COL_HEADER_H; j++)
                put_pixel(x + CH_WIDTH - 1, COL_HEADER_Y + j, COL_BORDER);
    }

    // Pattern grid
    for (int row = 0; row < ROWS_PER_PAT; row++) {
        int y = GRID_TOP_Y + row * ROW_H;
        // Row background
        uint16_t row_bg;
        if (playing && row == play_row && cur_pat == play_pat)
            row_bg = COL_PLAY_ROW;
        else if (row % 8 == 0)
            row_bg = COL_GRID_ALT;
        else
            row_bg = COL_GRID_BG;
        fill_rect(0, y, SCREEN_W, ROW_H, row_bg);

        // Row number
        buf[0] = hex1(row >> 4);
        buf[1] = hex1(row);
        buf[2] = 0;
        draw_str(ROW_NUM_X, y, buf, (row%8==0) ? COL_NOTE_C : COL_DIM);

        // Each channel
        for (int c = 0; c < NUM_CH; c++) {
            int x = ch_x[c];
            Cell* cell = &patterns[cur_pat][row][c];

            // Cursor highlight
            int is_cursor_row = (row == cur_row && c == cur_col);
            if (is_cursor_row)
                fill_rect(x - 1, y, CH_WIDTH - 1, ROW_H, COL_CURSOR);

            // Note
            uint16_t note_col;
            if (is_cursor_row)
                note_col = COL_BG;
            else if (cell->note == NOTE_OFF)
                note_col = COL_NOTE_OFF;
            else if (cell->note != NOTE_EMPTY && (cell->note-1)%12 == 0)
                note_col = COL_NOTE_C;
            else
                note_col = COL_NOTE;

            char note_s[5];
            note_to_str(cell->note, note_s);
            draw_str(x, y, note_s, note_col);

            // Instrument
            if (cell->note != NOTE_EMPTY && cell->note != NOTE_OFF) {
                uint16_t icol = is_cursor_row ? COL_BG : ch_cols[cell->inst];
                buf[0] = hex1(cell->inst);
                buf[1] = 0;
                draw_str(x + 24, y, buf, icol);

                // Volume
                uint16_t vcol = is_cursor_row ? COL_BG : COL_VOL;
                buf[0] = hex1(cell->vol >> 4);
                buf[1] = hex1(cell->vol);
                buf[2] = 0;
                draw_str(x + 32, y, buf, vcol);
            }

            // Separator
            if (c < NUM_CH-1)
                for (int j = 0; j < ROW_H; j++)
                    put_pixel(ch_x[c] + CH_WIDTH - 1, y + j, COL_BORDER);
        }
    }

    // VU meters
    for (int c = 0; c < NUM_CH; c++) draw_vu(c);

    // VU label
    draw_str(2, VU_Y + VU_H/2 - 3, "VU:", COL_DIM);

    // VU decay
    for (int c = 0; c < NUM_CH; c++) {
        if (vu_meter[c] > 0) vu_meter[c] -= 3;
        if (vu_meter[c] < 0) vu_meter[c] = 0;

        // Peak
        if (vu_meter[c] > vu_peak[c]) {
            vu_peak[c] = vu_meter[c];
            vu_peak_hold[c] = 30;
        }
        if (vu_peak_hold[c] > 0) vu_peak_hold[c]--;
        else if (vu_peak[c] > 0) vu_peak[c]--;
    }

    // Status bar
    fill_rect(0, STATUS_Y, SCREEN_W, 9, COL_HEADER_BG);
    draw_hline(0, STATUS_Y, SCREEN_W, COL_BORDER);

    // Instrument indicator
    draw_str(2, STATUS_Y + 1, "INST:", COL_DIM);
    static const char* inst_names[] = { "SQR", "TRI", "SAW", "NOI" };
    draw_str(32, STATUS_Y + 1, inst_names[cur_inst], ch_cols[cur_inst]);

    // Key hints
    draw_str(80, STATUS_Y + 1, "SPC:PLY/STP TAB:PAT DEL:CLR Z:OFF", COL_DIM);

    // Piano keyboard hint at bottom
    // row letter reference
    draw_str(2, STATUS_Y + 1 - 0, playing ? ">PLAYING" : " STOPPED", playing ? COL_PLAYING : COL_STOPPED);
}

// ---- Note entry keyboard map ----
// QWERTY row maps to piano keys:
//   Z=C  S=C# X=D  D=D# C=E  V=F  G=F# B=G  H=G# N=A  J=A# M=B
//   Q=C+ W=C#+ E=D+  R=D#+ T=E+  Y=F+  U=F#+ I=G+  O=G#+ P=A+

static int key_to_note_offset(int scancode) {
    // Returns semitone offset from C (current octave), -1 if not a note key
    switch(scancode) {
        case KEY_Z: return 0;   // C
        case KEY_S: return 1;   // C#
        case KEY_X: return 2;   // D
        case KEY_D: return 3;   // D#
        case KEY_C: return 4;   // E
        case KEY_V: return 5;   // F
        case KEY_G: return 6;   // F#
        case KEY_B: return 7;   // G
        case KEY_H: return 8;   // G#
        case KEY_N: return 9;   // A
        case KEY_J: return 10;  // A#
        case KEY_M: return 11;  // B
        case KEY_Q: return 12;  // C+1
        case KEY_2: return 13;  // C#+1
        case KEY_W: return 14;  // D+1
        case KEY_3: return 15;  // D#+1
        case KEY_E: return 16;  // E+1
        case KEY_R: return 17;  // F+1
        case KEY_5: return 18;  // F#+1
        case KEY_T: return 19;  // G+1
        case KEY_6: return 20;  // G#+1
        case KEY_Y: return 21;  // A+1
        case KEY_7: return 22;  // A#+1
        case KEY_U: return 23;  // B+1
        case KEY_I: return 24;  // C+2
    }
    return -1;
}

static int cur_octave = 4; // default octave

// ---- Default song data ----
static void init_default_song() {
    // Initialize song order
    for (int i = 0; i < 64; i++) song_order[i] = i % NUM_PATTERNS;
    song_length = 4;

    // Pattern 0: basic beat / melody (4 channels)
    // Channel 0: melody (square)
    static const uint8_t mel[] = {
        37,0,0,0, 41,0,0,0, 44,0,41,0, 0,0,0,0,
        37,0,0,0, 41,0,0,0, 44,0,46,0, 0,0,0,0,
        37,0,0,0, 41,0,0,0, 44,0,41,0, 0,0,0,0,
        39,0,0,0, 43,0,0,0, 46,0,44,0, 41,0,0,0
    };
    for (int i = 0; i < ROWS_PER_PAT; i++) {
        if (mel[i] != 0) {
            patterns[0][i][0].note = mel[i];
            patterns[0][i][0].inst = 0; // square
            patterns[0][i][0].vol = 48;
        }
    }

    // Channel 1: bass (sawtooth)
    static const uint8_t bass[] = {
        25,0,0,0, 0,0,0,0, 25,0,0,0, 0,0,0,0,
        27,0,0,0, 0,0,0,0, 27,0,0,0, 0,0,0,0,
        25,0,0,0, 0,0,0,0, 25,0,0,0, 0,0,0,0,
        24,0,0,0, 0,0,0,0, 24,0,0,0, 0,0,0,0
    };
    for (int i = 0; i < ROWS_PER_PAT; i++) {
        if (bass[i] != 0) {
            patterns[0][i][1].note = bass[i];
            patterns[0][i][1].inst = 2; // saw
            patterns[0][i][1].vol  = 40;
        }
    }

    // Channel 2: arpeggio (triangle)
    for (int i = 0; i < ROWS_PER_PAT; i++) {
        int n;
        switch(i % 4) {
            case 0: n = 37; break;
            case 1: n = 41; break;
            case 2: n = 44; break;
            default: n = 41; break;
        }
        if ((i/8) % 2 == 1) n += 2;
        patterns[0][i][2].note = n;
        patterns[0][i][2].inst = 1; // triangle
        patterns[0][i][2].vol  = 24;
    }

    // Channel 3: drums (noise)
    static const uint8_t drums[] = {
        1,0,0,0, 1,0,0,0, 1,0,0,1, 1,0,0,0,
        1,0,0,0, 1,0,0,0, 1,0,0,1, 1,0,0,0,
        1,0,0,0, 1,0,0,0, 1,0,0,1, 1,0,0,0,
        1,0,0,0, 1,0,0,0, 1,0,0,1, 1,0,1,0
    };
    for (int i = 0; i < ROWS_PER_PAT; i++) {
        if (drums[i] != 0) {
            patterns[0][i][3].note = 37; // pitch ~440Hz area for noise hit
            patterns[0][i][3].inst = 3;  // noise
            patterns[0][i][3].vol  = 56;
        }
    }

    // Pattern 1: variation
    for (int i = 0; i < ROWS_PER_PAT; i++) {
        patterns[1][i][0] = patterns[0][i][0];
        patterns[1][i][1] = patterns[0][i][1];
        patterns[1][i][2] = patterns[0][i][2];
        patterns[1][i][3] = patterns[0][i][3];
        // Transpose melody up
        if (patterns[1][i][0].note != NOTE_EMPTY && patterns[1][i][0].note != NOTE_OFF)
            patterns[1][i][0].note += 5;
    }
}

// ---- Main entry ----
__attribute__((visibility("default")))
void wupdate() {

    // Key events
    for (int i = 0; i < 256; i++) {
        key_just_pressed[i] = (_sys->keys[i] && !prev_keys[i]) ? 1 : 0;
        prev_keys[i] = _sys->keys[i];
    }

    int shift = _sys->keys[KEY_LSHIFT] || _sys->keys[KEY_RSHIFT];

    // Navigation
    if (key_just_pressed[KEY_UP]) {
        cur_row--;
        if (cur_row < 0) cur_row = ROWS_PER_PAT - 1;
    }
    if (key_just_pressed[KEY_DOWN]) {
        cur_row++;
        if (cur_row >= ROWS_PER_PAT) cur_row = 0;
    }
    if (key_just_pressed[KEY_LEFT]) {
        cur_col--;
        if (cur_col < 0) cur_col = NUM_CH - 1;
    }
    if (key_just_pressed[KEY_RIGHT]) {
        cur_col++;
        if (cur_col >= NUM_CH) cur_col = 0;
    }

    // Play/Stop
    if (key_just_pressed[KEY_SPACE]) {
        playing = !playing;
        if (playing) {
            play_song_pos = 0;
            play_pat = song_order[0];
            play_row = 0;
            audio_gen_sample_pos = 0;
        } else {
            // Release all channels
            for (int c = 0; c < NUM_CH; c++) {
                channels[c].releasing = 1;
                channels[c].env_rel = 0;
            }
        }
    }

    // Pattern navigation
    if (key_just_pressed[KEY_TAB]) {
        if (shift) {
            cur_pat--;
            if (cur_pat < 0) cur_pat = NUM_PATTERNS - 1;
        } else {
            cur_pat++;
            if (cur_pat >= NUM_PATTERNS) cur_pat = 0;
        }
    }

    // Instrument select via F1-F4
    if (key_just_pressed[KEY_F1]) cur_inst = 0;
    if (key_just_pressed[KEY_F2]) cur_inst = 1;
    if (key_just_pressed[KEY_F3]) cur_inst = 2;
    if (key_just_pressed[KEY_F4]) cur_inst = 3;

    // Octave control: +/- with numpad or brackets
    if (key_just_pressed[KEY_O]) { cur_octave++; if (cur_octave > 7) cur_octave = 7; }
    if (key_just_pressed[KEY_L]) { cur_octave--; if (cur_octave < 1) cur_octave = 1; }

    // Note off
    if (key_just_pressed[KEY_Z] && shift) {
        patterns[cur_pat][cur_row][cur_col].note = NOTE_OFF;
        cur_row++;
        if (cur_row >= ROWS_PER_PAT) cur_row = 0;
    }

    // Delete cell
    if (key_just_pressed[KEY_DELETE]) {
        patterns[cur_pat][cur_row][cur_col].note = NOTE_EMPTY;
        patterns[cur_pat][cur_row][cur_col].vol  = 0;
    }

    // Note entry
    if (!shift) {
        for (int sc = 0; sc < 256; sc++) {
            if (!key_just_pressed[sc]) continue;
            int offset = key_to_note_offset(sc);
            if (offset < 0) continue;
            // note = (octave-1)*12 + offset%12 + 1 + extra octave from offset>=12
            int note = (cur_octave - 1) * 12 + offset + 1;
            if (note < 1) note = 1;
            if (note > 96) note = 96;
            patterns[cur_pat][cur_row][cur_col].note = (uint8_t)note;
            patterns[cur_pat][cur_row][cur_col].inst = (uint8_t)cur_inst;
            patterns[cur_pat][cur_row][cur_col].vol  = 64;
            // Preview the note
            trigger_note(cur_col, (uint8_t)note, cur_inst, 64);
            cur_row++;
            if (cur_row >= ROWS_PER_PAT) cur_row = 0;
            break;
        }
    }

    // BPM control: +/-
    if (key_just_pressed[KEY_ENTER] && shift) {
        bpm++;
        if (bpm > 255) bpm = 255;
        samples_per_row = (SAMPLE_RATE * 60) / (bpm * ticks_per_row / 2);
    }
    if (key_just_pressed[KEY_BACKSP] && shift) {
        bpm--;
        if (bpm < 32) bpm = 32;
        samples_per_row = (SAMPLE_RATE * 60) / (bpm * ticks_per_row / 2);
    }

    // Audio
    fill_audio();

    // Draw
    draw_frame();
    _sig[0] = 1;
}

void winit() {
    _sys->width = 320;
    _sys->height = 240;
    _sys->signal_count = 4;
    _fb = (uint16_t*)(512 + _sys->signal_count);
    const char* t = "Wagnostic Tracker";
    for (int i = 0; i < 127 && t[i]; i++) ((char*)_sys->message)[i] = t[i];
    _sig[1] = 3;

    samples_per_row = (44100 * 60) / (bpm * ticks_per_row / 2);
    
    uint8_t* mem = (uint8_t*)0;
    audio_buf_ptr = mem + 512 + (320 * 240 * 2);
    
    for (int i = 0; i < 4; i++) {
        channels[i].vol = 128;
        channels[i].inst = INST_SQUARE;
        channels[i].active = 0;
    }
    
    init_default_song();

    // Start playing by default
    playing = 1;
    play_song_pos = 0;
    play_pat = song_order[0];
    play_row = 0;
}
