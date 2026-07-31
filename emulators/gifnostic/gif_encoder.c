#include "gif_encoder.h"
#include <stdlib.h>
#include <string.h>

struct GIFEncoder {
    FILE* fp;
    uint16_t width;
    uint16_t height;
    uint32_t frame_count;
};

typedef struct {
    uint8_t r, g, b;
} RGBColor;

typedef struct {
    RGBColor colors[256];
    int count;
} Palette;

typedef struct {
    FILE* fp;
    uint8_t block[255];
    int block_len;
    uint32_t bit_buf;
    int bit_count;
} BitStream;

static void flush_block(BitStream* bs) {
    if (bs->block_len > 0) {
        fputc((uint8_t)bs->block_len, bs->fp);
        fwrite(bs->block, 1, bs->block_len, bs->fp);
        bs->block_len = 0;
    }
}

static void put_bits(BitStream* bs, uint32_t code, int num_bits) {
    bs->bit_buf |= (code << bs->bit_count);
    bs->bit_count += num_bits;
    while (bs->bit_count >= 8) {
        bs->block[bs->block_len++] = (uint8_t)(bs->bit_buf & 0xFF);
        bs->bit_buf >>= 8;
        bs->bit_count -= 8;
        if (bs->block_len == 255) {
            flush_block(bs);
        }
    }
}

static void flush_bits(BitStream* bs) {
    if (bs->bit_count > 0) {
        bs->block[bs->block_len++] = (uint8_t)(bs->bit_buf & 0xFF);
        bs->bit_buf = 0;
        bs->bit_count = 0;
    }
    flush_block(bs);
    fputc(0x00, bs->fp);
}

#define HASH_SIZE 4096

typedef struct {
    uint32_t rgb;
    int index;
} ColorHashNode;

static int build_palette(const uint8_t* rgb24, size_t num_pixels, Palette* pal, uint8_t* out_indices) {
    ColorHashNode hash_table[HASH_SIZE];
    memset(hash_table, 0xFF, sizeof(hash_table));

    pal->count = 0;
    int overflow = 0;

    for (size_t i = 0; i < num_pixels; i++) {
        uint8_t r = rgb24[i * 3 + 0];
        uint8_t g = rgb24[i * 3 + 1];
        uint8_t b = rgb24[i * 3 + 2];
        uint32_t rgb = (r << 16) | (g << 8) | b;

        uint32_t hash = (rgb ^ (rgb >> 8)) % HASH_SIZE;
        int found_idx = -1;

        while (hash_table[hash].index != -1) {
            if (hash_table[hash].rgb == rgb) {
                found_idx = hash_table[hash].index;
                break;
            }
            hash = (hash + 1) % HASH_SIZE;
        }

        if (found_idx != -1) {
            out_indices[i] = (uint8_t)found_idx;
        } else {
            if (pal->count < 256) {
                int idx = pal->count;
                pal->colors[idx] = (RGBColor){r, g, b};
                pal->count++;
                hash_table[hash].rgb = rgb;
                hash_table[hash].index = idx;
                out_indices[i] = (uint8_t)idx;
            } else {
                overflow = 1;
                break;
            }
        }
    }

    if (!overflow) {
        return pal->count;
    }

    pal->count = 0;
    for (int r = 0; r < 6; r++) {
        for (int g = 0; g < 7; g++) {
            for (int b = 0; b < 6; b++) {
                pal->colors[pal->count++] = (RGBColor){
                    (uint8_t)((r * 255) / 5),
                    (uint8_t)((g * 255) / 6),
                    (uint8_t)((b * 255) / 5)
                };
            }
        }
    }

    for (size_t i = 0; i < num_pixels; i++) {
        int r_idx = (rgb24[i * 3 + 0] * 5 + 127) / 255;
        int g_idx = (rgb24[i * 3 + 1] * 6 + 127) / 255;
        int b_idx = (rgb24[i * 3 + 2] * 5 + 127) / 255;
        out_indices[i] = (uint8_t)(r_idx * 42 + g_idx * 6 + b_idx);
    }

    return pal->count;
}

#define LZW_HASH_SIZE 16384

typedef struct {
    int32_t key;
    uint16_t code;
} LZWNode;

static void lzw_compress(FILE* fp, int min_code_size, const uint8_t* indices, size_t num_pixels) {
    fputc((uint8_t)min_code_size, fp);

    BitStream bs;
    memset(&bs, 0, sizeof(bs));
    bs.fp = fp;

    int clear_code = 1 << min_code_size;
    int eoi_code = clear_code + 1;
    int next_code = eoi_code + 1;
    int curr_code_size = min_code_size + 1;

    LZWNode hash_table[LZW_HASH_SIZE];
    memset(hash_table, 0xFF, sizeof(hash_table));

    put_bits(&bs, clear_code, curr_code_size);

    if (num_pixels == 0) {
        put_bits(&bs, eoi_code, curr_code_size);
        flush_bits(&bs);
        return;
    }

    int32_t prefix = indices[0];

    for (size_t i = 1; i < num_pixels; i++) {
        uint8_t c = indices[i];
        int32_t key = (prefix << 8) | c;

        uint32_t hash = ((uint32_t)key ^ 0x55555555) % LZW_HASH_SIZE;
        int found_code = -1;

        while (hash_table[hash].key != -1) {
            if (hash_table[hash].key == key) {
                found_code = hash_table[hash].code;
                break;
            }
            hash = (hash + 1) % LZW_HASH_SIZE;
        }

        if (found_code != -1) {
            prefix = found_code;
        } else {
            put_bits(&bs, (uint32_t)prefix, curr_code_size);

            if (next_code < 4096) {
                hash_table[hash].key = key;
                hash_table[hash].code = (uint16_t)next_code;
                next_code++;

                if (next_code == (1 << curr_code_size) + 1 && curr_code_size < 12) {
                    curr_code_size++;
                }
            } else {
                put_bits(&bs, (uint32_t)clear_code, curr_code_size);
                memset(hash_table, 0xFF, sizeof(hash_table));
                next_code = eoi_code + 1;
                curr_code_size = min_code_size + 1;
            }

            prefix = c;
        }
    }

    put_bits(&bs, (uint32_t)prefix, curr_code_size);
    put_bits(&bs, (uint32_t)eoi_code, curr_code_size);
    flush_bits(&bs);
}

GIFEncoder* gif_create(const char* filename, uint16_t width, uint16_t height, int loop_count) {
    if (!filename || width == 0 || height == 0) return NULL;

    FILE* fp = fopen(filename, "wb");
    if (!fp) return NULL;

    GIFEncoder* gif = (GIFEncoder*)calloc(1, sizeof(GIFEncoder));
    if (!gif) {
        fclose(fp);
        return NULL;
    }

    gif->fp = fp;
    gif->width = width;
    gif->height = height;

    fwrite("GIF89a", 1, 6, fp);

    fputc(width & 0xFF, fp);
    fputc((width >> 8) & 0xFF, fp);
    fputc(height & 0xFF, fp);
    fputc((height >> 8) & 0xFF, fp);
    fputc(0x70, fp);
    fputc(0x00, fp);
    fputc(0x00, fp);

    if (loop_count >= 0) {
        uint8_t netscape_ext[] = {
            0x21, 0xFF, 0x0B,
            'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
            0x03, 0x01,
            (uint8_t)(loop_count & 0xFF),
            (uint8_t)((loop_count >> 8) & 0xFF),
            0x00
        };
        fwrite(netscape_ext, 1, sizeof(netscape_ext), fp);
    }

    return gif;
}

int gif_add_frame(GIFEncoder* gif, const uint8_t* rgb24_pixels, uint16_t delay_cs) {
    if (!gif || !gif->fp || !rgb24_pixels) return 0;

    size_t num_pixels = (size_t)gif->width * gif->height;
    uint8_t* indices = (uint8_t*)malloc(num_pixels);
    if (!indices) return 0;

    Palette pal;
    int colors_cnt = build_palette(rgb24_pixels, num_pixels, &pal, indices);

    int palette_bits = 2;
    while ((1 << palette_bits) < colors_cnt && palette_bits < 8) {
        palette_bits++;
    }

    uint8_t gce[] = {
        0x21, 0xF9, 0x04,
        0x04,
        (uint8_t)(delay_cs & 0xFF),
        (uint8_t)((delay_cs >> 8) & 0xFF),
        0x00,
        0x00
    };
    fwrite(gce, 1, sizeof(gce), gif->fp);

    uint8_t local_pal_flag = 0x80 | (palette_bits - 1);
    uint8_t img_desc[] = {
        0x2C,
        0x00, 0x00,
        0x00, 0x00,
        (uint8_t)(gif->width & 0xFF),
        (uint8_t)((gif->width >> 8) & 0xFF),
        (uint8_t)(gif->height & 0xFF),
        (uint8_t)((gif->height >> 8) & 0xFF),
        local_pal_flag
    };
    fwrite(img_desc, 1, sizeof(img_desc), gif->fp);

    int pal_entries = 1 << palette_bits;
    for (int i = 0; i < pal_entries; i++) {
        if (i < pal.count) {
            fputc(pal.colors[i].r, gif->fp);
            fputc(pal.colors[i].g, gif->fp);
            fputc(pal.colors[i].b, gif->fp);
        } else {
            fputc(0, gif->fp);
            fputc(0, gif->fp);
            fputc(0, gif->fp);
        }
    }

    int min_code_size = palette_bits < 2 ? 2 : palette_bits;
    lzw_compress(gif->fp, min_code_size, indices, num_pixels);

    free(indices);
    gif->frame_count++;
    return 1;
}

void gif_close(GIFEncoder* gif) {
    if (!gif) return;
    if (gif->fp) {
        fputc(0x3B, gif->fp);
        fclose(gif->fp);
    }
    free(gif);
}
