// ============================================================
//  CHIPUTNIK - Chiptune Tracker ROM for Wagnostic
//  Controls:
//    Arrow keys   = navigate grid
//    0-9 / A-F    = enter note/value (hex)
//    Space        = play/pause
//    Tab          = switch channel
//    Delete/Back  = clear cell
//    Z/X          = prev/next pattern
//    S            = toggle step record
//    Esc          = clear selection
// ============================================================

#include "wagnostic.h"

#define _sys W_SYS
#define _sig W_SIGNALS
static uint16_t* _fb;

// SDL scancodes we care about
#define KEY_RIGHT  79
#define KEY_LEFT   80
#define KEY_DOWN   81
#define KEY_UP     82
#define KEY_SPACE  44
#define KEY_TAB    43
#define KEY_DEL    76
#define KEY_BACK   42
#define KEY_Z      29
#define KEY_X      27
#define KEY_S      22
#define KEY_ENTER  40
#define KEY_ESC    41
#define KEY_F5     68

// Number row scancodes (SDL2)
#define KEY_0 39
#define KEY_1 30
#define KEY_2 31
#define KEY_3 32
#define KEY_4 33
#define KEY_5 34
#define KEY_6 35
#define KEY_7 36
#define KEY_8 37
#define KEY_9 38
// Letter keys for hex A-F
#define KEY_A  4
#define KEY_B  5
#define KEY_C  6
#define KEY_D  7
#define KEY_E  8
#define KEY_F  9

// ---- Colors (RGB565) ----
#define RGB565(r,g,b) (uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))

#define COL_BG       RGB565(12,12,18)
#define COL_PANEL    RGB565(20,20,32)
#define COL_HEADER   RGB565(30,30,50)
#define COL_CURSOR   RGB565(255,200,0)
#define COL_PLAYING  RGB565(0,220,120)
#define COL_NOTE     RGB565(100,180,255)
#define COL_VOL      RGB565(80,220,140)
#define COL_INST     RGB565(220,140,80)
#define COL_FX       RGB565(200,100,200)
#define COL_EMPTY    RGB565(50,55,70)
#define COL_WHITE    RGB565(220,220,220)
#define COL_GRAY     RGB565(100,100,120)
#define COL_DARK     RGB565(25,25,40)
#define COL_SEL      RGB565(60,50,80)
#define COL_CH0      RGB565(80,160,255)
#define COL_CH1      RGB565(100,220,120)
#define COL_CH2      RGB565(220,160,80)
#define COL_CH3      RGB565(200,100,200)

// ---- Tracker data model ----
#define NUM_CHANNELS  4
#define NUM_ROWS      32
#define NUM_PATTERNS  8
#define SONG_LENGTH   16

typedef struct {
    uint8_t note;    // 0=empty, 1-96=C0..B7
    uint8_t inst;    // instrument 0-15
    uint8_t vol;     // 0-15 (0=default)
    uint8_t fx;      // effect 0-15
    uint8_t fxparam; // 0-255
} Cell;

typedef struct {
    Cell rows[NUM_ROWS][NUM_CHANNELS];
} Pattern;

// Instrument: simple synth params
typedef struct {
    uint8_t wave;    // 0=square, 1=saw, 2=tri, 3=noise
    uint8_t attack;  // 0-15
    uint8_t decay;   // 0-15
    uint8_t sustain; // 0-15
    uint8_t release; // 0-15
    uint8_t duty;    // 0-15 (for square wave pulse width)
} Instrument;

static Pattern  patterns[NUM_PATTERNS];
static uint8_t  song[SONG_LENGTH];   // pattern index per song step
static Instrument instruments[16];

// ---- Playback state ----
#define SAMPLE_RATE  44100
#define AUDIO_SIZE   32768

typedef struct {
    float    phase;
    float    freq;
    int      env_stage;   // 0=off,1=attack,2=decay,3=sustain,4=release
    uint32_t env_timer;
    float    env_vol;
    float    target_vol;
    uint8_t  inst_idx;
    uint8_t  note;
    int      active;
} Voice;

static Voice voices[NUM_CHANNELS];

static int     playing       = 0;
static int     song_pos      = 0;
static int     row_pos       = 0;
static uint32_t ticks_per_row = (SAMPLE_RATE * 60) / (120 * 4); // 120 BPM, 1/16
static uint32_t row_timer    = 0;
static uint8_t  prev_keys[256];
static uint32_t last_ticks   = 0;

// ---- Editor state ----
static int cur_pattern  = 0;
static int cur_row      = 0;
static int cur_chan      = 0;
static int cur_field     = 0; // 0=note,1=inst,2=vol,3=fx,4=fxp
static int step_record   = 0;
static int edit_digit    = 0; // which nibble being edited
static int view_offset   = 0; // scroll row

// ---- Note names ----
static const char* NOTE_NAMES[13] = {
    "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-","---"
};

// ---- Frequencies ----
// C4 = 261.63 Hz, ratio = 2^(1/12)
// note 1 = C0, note 49 = C4
static float note_to_freq(uint8_t note) {
    if (note == 0) return 0.0f;
    // C0 = 16.35 Hz
    // freq = 16.35 * 2^((note-1)/12)
    float n = (float)(note - 1);
    // 2^(n/12): use repeated multiplication for approximation
    float freq = 16.3516f;
    // fast exp2 approximation
    int oct = (int)(n / 12.0f);
    float semi = n - (float)(oct * 12);
    // semitone ratios: 1, 1.0595, 1.1225, ...
    static const float semitones[12] = {
        1.0000f, 1.0595f, 1.1225f, 1.1892f, 1.2599f, 1.3348f,
        1.4142f, 1.4983f, 1.5874f, 1.6818f, 1.7818f, 1.8877f
    };
    int si = (int)semi;
    if (si < 0) si = 0;
    if (si > 11) si = 11;
    freq *= semitones[si];
    for (int i = 0; i < oct; i++) freq *= 2.0f;
    return freq;
}

// ---- Waveform generators ----
static float gen_sample(int ch) {
    Voice* v = &voices[ch];
    if (!v->active) return 0.0f;

    Instrument* ins = &instruments[v->inst_idx & 0xF];
    float s = 0.0f;

    switch (ins->wave & 3) {
    case 0: // Square
        {
            float duty = 0.1f + (ins->duty / 15.0f) * 0.8f;
            s = (v->phase < duty) ? 1.0f : -1.0f;
        }
        break;
    case 1: // Saw
        s = 2.0f * v->phase - 1.0f;
        break;
    case 2: // Triangle
        s = (v->phase < 0.5f) ? (4.0f * v->phase - 1.0f) : (3.0f - 4.0f * v->phase);
        break;
    case 3: // Noise (LCG)
        {
            static uint32_t seed[4] = {12345,54321,11111,99999};
            seed[ch] = seed[ch] * 1664525 + 1013904223;
            s = ((float)(int32_t)seed[ch]) / 2147483648.0f;
        }
        break;
    }

    // Envelope
    float env = v->env_vol;
    uint32_t a_time = (uint32_t)(ins->attack  * 1000);
    uint32_t d_time = (uint32_t)(ins->decay   * 1000);
    uint32_t r_time = (uint32_t)(ins->release * 1000);
    float    s_vol  = ins->sustain / 15.0f;

    if (v->env_stage == 1) { // attack
        if (a_time == 0) { env = 1.0f; v->env_stage = 2; }
        else {
            env = (float)v->env_timer / (float)a_time;
            if (env >= 1.0f) { env = 1.0f; v->env_stage = 2; v->env_timer = 0; }
        }
    } else if (v->env_stage == 2) { // decay
        if (d_time == 0) { env = s_vol; v->env_stage = 3; }
        else {
            env = 1.0f - (1.0f - s_vol) * ((float)v->env_timer / (float)d_time);
            if (v->env_timer >= d_time) { env = s_vol; v->env_stage = 3; v->env_timer = 0; }
        }
    } else if (v->env_stage == 3) { // sustain
        env = s_vol;
    } else if (v->env_stage == 4) { // release
        if (r_time == 0) { env = 0.0f; v->active = 0; }
        else {
            env = v->target_vol * (1.0f - (float)v->env_timer / (float)r_time);
            if (env < 0.0f || v->env_timer >= r_time) { env = 0.0f; v->active = 0; }
        }
    }
    v->env_vol = env;
    v->env_timer++;

    v->phase += v->freq / (float)SAMPLE_RATE;
    while (v->phase >= 1.0f) v->phase -= 1.0f;

    return s * env * 0.22f;
}

// ---- Trigger a note ----
static void trigger_note(int ch, uint8_t note, uint8_t inst, uint8_t vol) {
    Voice* v = &voices[ch];
    v->note     = note;
    v->inst_idx = inst & 0xF;
    v->freq     = note_to_freq(note);
    v->phase    = 0.0f;
    v->env_stage= 1;
    v->env_timer= 0;
    v->env_vol  = 0.0f;
    v->target_vol = (vol > 0) ? (vol / 15.0f) : 1.0f;
    v->active   = (note > 0) ? 1 : 0;
}

// ---- Advance playback by one row ----
static void advance_row() {
    int pat_idx = song[song_pos % SONG_LENGTH];
    Pattern* p = &patterns[pat_idx];

    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        Cell* c = &p->rows[row_pos][ch];
        if (c->note > 0) {
            trigger_note(ch, c->note, c->inst, c->vol);
        }
        if (c->fx == 0xE) {
            voices[ch].env_stage = 4;
            voices[ch].env_timer = 0;
            voices[ch].target_vol = voices[ch].env_vol;
        }
    }

    row_pos++;
    if (row_pos >= NUM_ROWS) {
        row_pos = 0;
        song_pos++;
        if (song_pos >= SONG_LENGTH) song_pos = 0;

        // Note-off all active voices at every pattern boundary
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            if (voices[ch].active) {
                voices[ch].env_stage = 4;
                voices[ch].env_timer = 0;
                voices[ch].target_vol = voices[ch].env_vol;
            }
        }
    }
}

// ---- Fill audio buffer ----
static void fill_audio() {
    uint8_t* mem = (uint8_t*)0;
    uint8_t* audio_buf = mem + 512 + (_sys->width * _sys->height * 2);

    uint32_t r = _sys->audio_read;
    uint32_t w = _sys->audio_write;
    uint32_t size = _sys->audio_size;

    int avail = (r > w) ? (int)(r - w - 1) : (int)(size - w + r - 1);
    if (avail < 0) avail = 0;
    int to_write = avail / 4;
    if (to_write > 1024) to_write = 1024;

    for (int i = 0; i < to_write; i++) {
        int16_t s16 = 0;

        if (playing) {
            if (row_timer == 0) advance_row();
            row_timer++;
            if (row_timer >= ticks_per_row) row_timer = 0;

            float mix = 0.0f;
            for (int ch = 0; ch < NUM_CHANNELS; ch++) mix += gen_sample(ch);
            if (mix >  1.0f) mix =  1.0f;
            if (mix < -1.0f) mix = -1.0f;
            s16 = (int16_t)(mix * 30000.0f);
        }
        // When paused, s16 = 0 (silence) — keeps the ring buffer fed
        // so SDL never stalls waiting for data

        uint32_t pos = (w + (uint32_t)i * 4) % size;
        int16_t* out = (int16_t*)(audio_buf + pos);
        out[0] = s16;
        out[1] = s16;
    }
    _sys->audio_write = (w + (uint32_t)to_write * 4) % size;
}

// ---- Drawing helpers ----
static void fill_rect(int x, int y, int w, int h, uint16_t c) {
    for (int iy = y; iy < y+h; iy++)
        for (int ix = x; ix < x+w; ix++)
            if (ix>=0 && ix<320 && iy>=0 && iy<240)
                _fb[iy*320+ix] = c;
}

// Tiny 4x5 pixel font
static const uint8_t FONT[128][5] = {
    [' '] = {0,0,0,0,0},
    ['-'] = {0,0,14,0,0},
    ['0'] = {14,17,17,17,14},
    ['1'] = {4,12,4,4,14},
    ['2'] = {14,1,14,16,31},
    ['3'] = {14,1,6,1,14},
    ['4'] = {17,17,31,1,1},
    ['5'] = {31,16,30,1,30},
    ['6'] = {14,16,30,17,14},
    ['7'] = {31,1,2,4,4},
    ['8'] = {14,17,14,17,14},
    ['9'] = {14,17,15,1,14},
    ['A'] = {14,17,31,17,17},
    ['B'] = {30,17,30,17,30},
    ['C'] = {14,16,16,16,14},
    ['D'] = {30,17,17,17,30},
    ['E'] = {31,16,30,16,31},
    ['F'] = {31,16,30,16,16},
    ['G'] = {14,16,19,17,14},
    ['H'] = {17,17,31,17,17},
    ['I'] = {14,4,4,4,14},
    ['J'] = {7,2,2,18,12},
    ['K'] = {17,18,28,18,17},
    ['L'] = {16,16,16,16,31},
    ['M'] = {17,27,21,17,17},
    ['N'] = {17,25,21,19,17},
    ['O'] = {14,17,17,17,14},
    ['P'] = {30,17,30,16,16},
    ['Q'] = {14,17,17,19,15},
    ['R'] = {30,17,30,18,17},
    ['S'] = {14,16,14,1,14},
    ['T'] = {31,4,4,4,4},
    ['U'] = {17,17,17,17,14},
    ['V'] = {17,17,17,10,4},
    ['W'] = {17,17,21,27,17},
    ['X'] = {17,10,4,10,17},
    ['Y'] = {17,10,4,4,4},
    ['Z'] = {31,2,4,8,31},
    ['#'] = {10,31,10,31,10},
    [':'] = {0,4,0,4,0},
    ['>'] = {8,4,2,4,8},
    ['|'] = {4,4,4,4,4},
    ['.'] = {0,0,0,0,4},
    ['?'] = {14,1,6,0,4},
    ['/'] = {1,2,4,8,16},
    ['['] = {6,4,4,4,6},
    [']'] = {12,4,4,4,12},
};

static void draw_char(int x, int y, char ch, uint16_t color) {
    if ((unsigned char)ch >= 128) return;
    const uint8_t* glyph = FONT[(unsigned char)ch];
    for (int row = 0; row < 5; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 5; col++) {
            if (bits & (16 >> col)) {
                int px = x + col, py = y + row;
                if (px>=0 && px<320 && py>=0 && py<240)
                    _fb[py*320+px] = color;
            }
        }
    }
}

static void draw_str(int x, int y, const char* s, uint16_t color) {
    while (*s) {
        draw_char(x, y, *s, color);
        x += 6;
        s++;
    }
}

// Hex digit to char
static char hex_ch(int v) {
    v &= 0xF;
    return (v < 10) ? ('0' + v) : ('A' + v - 10);
}

// Draw 2-digit hex
static void draw_hex2(int x, int y, uint8_t v, uint16_t col) {
    char buf[3] = { hex_ch(v>>4), hex_ch(v), 0 };
    draw_str(x, y, buf, col);
}

// ---- Note string ----
static void note_str(uint8_t note, char* out) {
    if (note == 0) {
        out[0]='.'; out[1]='.'; out[2]='.'; out[3]=0;
    } else {
        int n = (note - 1) % 12;
        int o = (note - 1) / 12;
        out[0] = NOTE_NAMES[n][0];
        out[1] = NOTE_NAMES[n][1];
        out[2] = '0' + o;
        out[3] = 0;
    }
}

// ---- Key edge detection ----
static int key_pressed(int sc) {
    return _sys->keys[sc] && !prev_keys[sc];
}

// ---- Channel colors ----
static uint16_t ch_color(int ch) {
    switch(ch) {
    case 0: return COL_CH0;
    case 1: return COL_CH1;
    case 2: return COL_CH2;
    default: return COL_CH3;
    }
}

// ---- UI layout constants ----
#define HEADER_H    14
#define ROW_H        7
#define CHAN_W       60
#define ROW_NUM_W   18
#define PATTERN_X    0
#define ROWS_VISIBLE 30

// field widths in pixels
static int field_x(int ch, int field) {
    int base = PATTERN_X + ROW_NUM_W + ch * CHAN_W;
    switch(field) {
    case 0: return base;        // note (18px)
    case 1: return base + 19;   // inst (12px)
    case 2: return base + 31;   // vol  (12px)
    case 3: return base + 43;   // fx   (6px)
    case 4: return base + 49;   // fxp  (12px)
    }
    return base;
}

// ---- Init default song ----
static void init_data() {
    // Clear all
    for (int p = 0; p < NUM_PATTERNS; p++)
        for (int r = 0; r < NUM_ROWS; r++)
            for (int c = 0; c < NUM_CHANNELS; c++) {
                patterns[p].rows[r][c].note = 0;
                patterns[p].rows[r][c].inst = 0;
                patterns[p].rows[r][c].vol  = 0;
                patterns[p].rows[r][c].fx   = 0;
                patterns[p].rows[r][c].fxparam = 0;
            }

    for (int i = 0; i < SONG_LENGTH; i++) song[i] = 0;

    // Default instruments
    for (int i = 0; i < 16; i++) {
        instruments[i].wave    = i & 3;
        instruments[i].attack  = 0;
        instruments[i].decay   = 3;
        instruments[i].sustain = 8;
        instruments[i].release = 4;
        instruments[i].duty    = 8;
    }

    // Demo pattern 0: simple arpeggio
    // Ch0: lead melody (square)
    uint8_t melody[] = {49,52,56,49, 51,55,58,51, 48,52,55,48, 46,50,53,46,
                        49,52,56,49, 51,55,58,51, 53,57,60,53, 52,0,0,0};
    for (int r = 0; r < NUM_ROWS; r++) {
        patterns[0].rows[r][0].note = melody[r];
        patterns[0].rows[r][0].inst = 0;
        patterns[0].rows[r][0].vol  = 12;
    }
    // Ch1: bass (saw)
    uint8_t bass[] = {37,0,0,0, 39,0,0,0, 36,0,0,0, 34,0,0,0,
                      37,0,0,0, 39,0,0,0, 41,0,0,0, 40,0,0,0};
    for (int r = 0; r < NUM_ROWS; r++) {
        patterns[0].rows[r][1].note = bass[r];
        patterns[0].rows[r][1].inst = 1;
        patterns[0].rows[r][1].vol  = 10;
    }
    // Ch2: hat/perc (noise)
    for (int r = 0; r < NUM_ROWS; r++) {
        if (r % 2 == 0) {
            patterns[0].rows[r][2].note = 60;
            patterns[0].rows[r][2].inst = 3;
            patterns[0].rows[r][2].vol  = 6;
            patterns[0].rows[r][2].fx   = 0xE; // note off fast
        }
    }

    for (int i = 0; i < SONG_LENGTH; i++) song[i] = 0;
}

// ---- Render the tracker UI ----
static void render() {
    // Background
    fill_rect(0,0,320,240, COL_BG);

    // Header bar
    fill_rect(0,0,320,HEADER_H, COL_HEADER);
    draw_str(3, 4, "CHIPUTNIK", COL_CURSOR);
    draw_str(70, 4, playing ? ">PLAY" : " STOP", playing ? COL_PLAYING : COL_GRAY);

    // BPM
    draw_str(120, 4, "BPM:", COL_GRAY);
    int bpm = SAMPLE_RATE * 60 / (ticks_per_row * 4);
    char bpm_str[8];
    bpm_str[0] = '0' + (bpm/100)%10;
    bpm_str[1] = '0' + (bpm/10)%10;
    bpm_str[2] = '0' + bpm%10;
    bpm_str[3] = 0;
    draw_str(148, 4, bpm_str, COL_WHITE);

    // Pattern / Song pos
    draw_str(175, 4, "PAT:", COL_GRAY);
    draw_hex2(199, 4, cur_pattern, COL_NOTE);
    draw_str(215, 4, "SNG:", COL_GRAY);
    draw_hex2(239, 4, song_pos, COL_PLAYING);

    // Step record indicator
    if (step_record) draw_str(258, 4, "[REC]", RGB565(220,60,60));

    // Channel headers
    int col_labels_y = HEADER_H + 1;
    fill_rect(0, col_labels_y, 320, 7, COL_PANEL);
    draw_str(1, col_labels_y+1, "ROW", COL_GRAY);
    const char* ch_names[] = {"CH1","CH2","CH3","CH4"};
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        int cx = PATTERN_X + ROW_NUM_W + ch * CHAN_W;
        fill_rect(cx, col_labels_y, CHAN_W-1, 7, COL_DARK);
        draw_str(cx+1, col_labels_y+1, ch_names[ch], ch_color(ch));
        // Voice activity indicator
        if (voices[ch].active) {
            fill_rect(cx+38, col_labels_y+2, 3, 3, ch_color(ch));
        }
    }

    // Pattern rows
    Pattern* pat = &patterns[cur_pattern];
    int rows_y = HEADER_H + 9;

    for (int vis = 0; vis < ROWS_VISIBLE; vis++) {
        int r = (view_offset + vis) % NUM_ROWS;
        int y = rows_y + vis * ROW_H;
        int is_playrow = playing && (r == (row_pos > 0 ? row_pos-1 : NUM_ROWS-1));
        int is_currow  = (r == cur_row);

        // Row background
        uint16_t bg = COL_BG;
        if (is_playrow) bg = RGB565(0,30,15);
        if (is_currow && !is_playrow) bg = COL_SEL;
        if (r % 4 == 0 && !is_currow && !is_playrow) bg = COL_PANEL;
        fill_rect(0, y, 320, ROW_H-1, bg);

        // Row number
        char rn[3] = { hex_ch(r>>4), hex_ch(r), 0 };
        uint16_t rn_col = (r % 4 == 0) ? COL_WHITE : COL_GRAY;
        if (is_playrow) rn_col = COL_PLAYING;
        draw_str(1, y+1, rn, rn_col);

        // Cells
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            Cell* c = &pat->rows[r][ch];
            int base_x = PATTERN_X + ROW_NUM_W + ch * CHAN_W;

            // Highlight cursor cell
            if (is_currow && ch == cur_chan) {
                int fx_x = field_x(ch, cur_field);
                int fw = (cur_field == 0) ? 18 : (cur_field == 4 ? 12 : (cur_field == 3 ? 6 : 12));
                fill_rect(fx_x, y, fw, ROW_H-1, RGB565(70,60,20));
                // cursor highlight
                fill_rect(fx_x, y, fw, 1, COL_CURSOR);
            }

            // Note
            char ns[4]; note_str(c->note, ns);
            uint16_t nc = (c->note == 0) ? COL_EMPTY : ch_color(ch);
            draw_str(base_x, y+1, ns, nc);

            // Inst
            if (c->note > 0) {
                draw_char(base_x+19, y+1, hex_ch(c->inst), COL_INST);
            } else {
                draw_char(base_x+19, y+1, '.', COL_EMPTY);
            }

            // Vol
            if (c->vol > 0) {
                draw_char(base_x+25, y+1, hex_ch(c->vol), COL_VOL);
            } else {
                draw_char(base_x+25, y+1, '.', COL_EMPTY);
            }

            // Fx
            if (c->fx > 0) {
                draw_char(base_x+31, y+1, hex_ch(c->fx), COL_FX);
                // fxparam
                draw_char(base_x+37, y+1, hex_ch(c->fxparam>>4), COL_FX);
                draw_char(base_x+43, y+1, hex_ch(c->fxparam), COL_FX);
            } else {
                draw_str(base_x+31, y+1, "...", COL_EMPTY);
            }

            // Channel separator
            if (ch < NUM_CHANNELS-1)
                fill_rect(base_x+CHAN_W-1, y, 1, ROW_H-1, COL_DARK);
        }
    }

    // Song bar (bottom)
    int sb_y = 232;
    fill_rect(0, sb_y, 320, 8, COL_PANEL);
    draw_str(1, sb_y+2, "SONG:", COL_GRAY);
    for (int i = 0; i < SONG_LENGTH; i++) {
        int sx = 32 + i * 18;
        uint16_t sc = (i == song_pos && playing) ? COL_PLAYING :
                      (song[i] > 0 ? COL_NOTE : COL_EMPTY);
        draw_hex2(sx, sb_y+2, song[i], sc);
    }

    // Status line
    int st_y = 222;
    fill_rect(0, st_y, 320, 9, COL_DARK);
    draw_str(1, st_y+2, "SPC:play Z/X:pat TAB:ch DEL:clear S:rec", COL_GRAY);
}

// ---- Input nibble to note entry ----
static int scan_to_hex(int sc) {
    switch(sc) {
    case KEY_0: return 0;
    case KEY_1: return 1;
    case KEY_2: return 2;
    case KEY_3: return 3;
    case KEY_4: return 4;
    case KEY_5: return 5;
    case KEY_6: return 6;
    case KEY_7: return 7;
    case KEY_8: return 8;
    case KEY_9: return 9;
    case KEY_A: return 10;
    case KEY_B: return 11;
    case KEY_C: return 12;
    case KEY_D: return 13;
    case KEY_E: return 14;
    case KEY_F: return 15;
    }
    return -1;
}

// Note input via letters: Q=C, W=D, E=E, R=F, T=G, Y=A, U=B, I=C+oct
// Scancodes: Q=20, W=26, E=8(clash E=KEY_E), R=21, T=23, Y=28, U=24, I=12
// Octave: Z/X handled separately for pattern nav, use keys 3/4 for octave
static int cur_octave = 4;

static int scan_to_note(int sc) {
    // Maps to semitone offset within octave
    switch(sc) {
    case 20: return 0;  // Q=C
    case 26: return 2;  // W=D
    // E=8 but KEY_E=8 (conflict) - skip E key for note
    case 21: return 5;  // R=F
    case 23: return 7;  // T=G
    case 28: return 9;  // Y=A
    case 24: return 11; // U=B
    case 12: return 12; // I=C+1
    case 14: return 1;  // K... no: map s/d/f/g/h/j for black keys
    // Black keys: S=C#, D=D#, G=F#, H=G#, J=A#
    case 22: return 1;  // S=C#  (conflict: KEY_S=step record)
    // Use different keys to avoid conflicts:
    // Let's use numpad-style: layout on keyboard
    // Q W E R T Y U I = C D E F G A B C
    // Actually just map a clean set:
    }
    return -1;
}

// Simpler note input: number keys 1-8 change octave, letter keys = notes
static int get_note_from_key() {
    // octave change
    if (key_pressed(KEY_3)) { cur_octave--; if (cur_octave<0) cur_octave=0; }
    if (key_pressed(KEY_4)) { cur_octave++; if (cur_octave>7) cur_octave=7; }

    // note row: QWERTY for white keys, row above for black
    // White: Q=C, W=D, (E=conflict), R=F, T=G, Y=A, U=B
    // Use A=C is conflict too. Let's use:
    // Z(29)=C, X(27)=D, C(6)=conflict, V(25)=E, B(5)=F, N(17)=G, M(16)=A, ,(54)=B
    // Or simply just Q/W/R/T/Y/U for notes:
    struct { int sc; int semi; } map[] = {
        {20, 0},  // Q = C
        {26, 2},  // W = D
        {8,  4},  // E = E  (same as KEY_E hex=14... both can't easily coexist)
        {21, 5},  // R = F
        {23, 7},  // T = G
        {28, 9},  // Y = A
        {24, 11}, // U = B
        {12, 12}, // I = C+1
        // Black keys row above: 2=C#, 3/4 used for octave... use F1-row
        // Simple: just skip black keys for now, they can enter via hex
        {-1, -1}
    };
    for (int i = 0; map[i].sc >= 0; i++) {
        if (map[i].sc != KEY_S && key_pressed(map[i].sc)) {
            int note = cur_octave * 12 + map[i].semi + 1;
            if (note > 96) note = 96;
            return note;
        }
    }
    return -1;
}

// ---- Handle input ----
static void handle_input() {
    // Navigation
    if (key_pressed(KEY_DOWN)) {
        cur_row++;
        if (cur_row >= NUM_ROWS) cur_row = 0;
    }
    if (key_pressed(KEY_UP)) {
        cur_row--;
        if (cur_row < 0) cur_row = NUM_ROWS - 1;
    }
    if (key_pressed(KEY_RIGHT)) {
        cur_field++;
        if (cur_field > 4) { cur_field = 0; cur_chan++; if (cur_chan >= NUM_CHANNELS) { cur_chan = 0; } }
    }
    if (key_pressed(KEY_LEFT)) {
        cur_field--;
        if (cur_field < 0) { cur_field = 4; cur_chan--; if (cur_chan < 0) { cur_chan = NUM_CHANNELS-1; } }
    }
    if (key_pressed(KEY_TAB)) {
        cur_chan = (cur_chan + 1) % NUM_CHANNELS;
        cur_field = 0;
    }

    // Pattern navigation
    if (key_pressed(KEY_Z)) { cur_pattern--; if (cur_pattern<0) cur_pattern=NUM_PATTERNS-1; }
    if (key_pressed(KEY_X)) { cur_pattern++; if (cur_pattern>=NUM_PATTERNS) cur_pattern=0; }

    // Play/stop
    if (key_pressed(KEY_SPACE)) {
        playing ^= 1;
        if (playing) {
            row_timer = 0;
            // Full voice reset on resume to clear any stale envelope state
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                voices[ch].active    = 0;
                voices[ch].env_stage = 0;
                voices[ch].env_vol   = 0.0f;
                voices[ch].env_timer = 0;
            }
        } else {
            // Kill all voices immediately
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                voices[ch].active = 0;
                voices[ch].env_stage = 0;
                voices[ch].env_vol = 0.0f;
            }
            // Drain the ring buffer so SDL stops playing stale audio
            _sys->audio_read = _sys->audio_write;
        }
    }

    // Step record toggle
    if (key_pressed(KEY_S)) step_record ^= 1;

    // BPM adjust: + and - via [ and ]
    if (_sys->keys[47] && !prev_keys[47]) { // - key (scancode 45)
        ticks_per_row++;
    }
    if (_sys->keys[45] && !prev_keys[45]) { // = key (scancode 46)
        if (ticks_per_row > 1) ticks_per_row--;
    }

    // Delete cell
    if (key_pressed(KEY_DEL) || key_pressed(KEY_BACK)) {
        Cell* c = &patterns[cur_pattern].rows[cur_row][cur_chan];
        switch(cur_field) {
        case 0: c->note = 0; c->inst = 0; c->vol = 0; break;
        case 1: c->inst = 0; break;
        case 2: c->vol = 0; break;
        case 3: c->fx = 0; c->fxparam = 0; break;
        case 4: c->fxparam = 0; break;
        }
    }

    // Note entry (field 0)
    Cell* cell = &patterns[cur_pattern].rows[cur_row][cur_chan];
    if (cur_field == 0) {
        int note = get_note_from_key();
        if (note > 0) {
            cell->note = (uint8_t)note;
            if (step_record) {
                trigger_note(cur_chan, cell->note, cell->inst, cell->vol ? cell->vol : 12);
                cur_row = (cur_row + 1) % NUM_ROWS;
            }
        }
    } else {
        // Hex entry for other fields
        int hex = -1;
        int hex_scancodes[] = {KEY_0,KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_6,KEY_7,KEY_8,KEY_9,KEY_A,KEY_B,KEY_C,KEY_D,KEY_E,KEY_F};
        int hex_vals[]       = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        for (int i = 0; i < 16; i++) {
            if (key_pressed(hex_scancodes[i])) { hex = hex_vals[i]; break; }
        }
        if (hex >= 0) {
            switch(cur_field) {
            case 1: cell->inst = (uint8_t)hex; break;
            case 2: cell->vol  = (uint8_t)hex; break;
            case 3: cell->fx   = (uint8_t)hex; break;
            case 4:
                // Two nibble entry
                cell->fxparam = (uint8_t)((cell->fxparam << 4) | hex);
                break;
            }
        }
    }

    // Adjust view offset to keep cursor visible
    if (cur_row < view_offset) view_offset = cur_row;
    if (cur_row >= view_offset + ROWS_VISIBLE) view_offset = cur_row - ROWS_VISIBLE + 1;
}

// ---- Main game loop ----
void wupdate() {

    handle_input();
    fill_audio();
    render();

    for (int i = 0; i < 256; i++) prev_keys[i] = _sys->keys[i];
    _sig[0] = 1; // REDRAW signal
}

void winit() {
    _sys->width = 320;
    _sys->height = 240;
    _sys->bpp = 16;
    _sys->scale = 3;
    // Signals are fixed now.
    _sys->audio_size = AUDIO_SIZE;
    _sys->audio_sample_rate = SAMPLE_RATE;
    _sys->audio_bpp = 2;
    _sys->audio_channels = 2;
    _fb = (uint16_t*)(512);
    const char* t = "Chiputnik - Chiptune Tracker";
    for (int i = 0; i < 127 && t[i]; i++) ((char*)_sys->message)[i] = t[i];
    _sig[1] = 3; // UPDATE_TITLE signal
    
    init_data();
    last_ticks = W_SYS->ticks;
}