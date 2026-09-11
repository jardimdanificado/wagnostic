#ifndef PIOLHO_GIF_H
#define PIOLHO_GIF_H

#include <stdint.h>

#define GIF_EXTENSION  "std:gif"
#define WGIF_EXTENSION "std:gif"

typedef struct {
    uint32_t recording;     /* 1 if host is actively recording GIF */
    uint32_t frame_count;   /* Captured frame count */
    uint32_t max_frames;    /* Max frame target (0 = unlimited) */
    uint32_t delay_cs;      /* Frame delay in centiseconds (1/100s) */
    uint32_t save_trigger;  /* ROM sets to 1 to request flush/capture */
} gif_t;

typedef gif_t wgif_t;

#endif /* PIOLHO_GIF_H */
