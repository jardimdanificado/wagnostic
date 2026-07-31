#ifndef GIF_ENCODER_H
#define GIF_ENCODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GIFEncoder GIFEncoder;

GIFEncoder* gif_create(const char* filename, uint16_t width, uint16_t height, int loop_count);
int gif_add_frame(GIFEncoder* gif, const uint8_t* rgb24_pixels, uint16_t delay_cs);
void gif_close(GIFEncoder* gif);

#ifdef __cplusplus
}
#endif

#endif /* GIF_ENCODER_H */
