#ifndef PIOLHO_GIF_H
#define PIOLHO_GIF_H

#include <stdint.h>

#define WGIF_EXTENSION "std:gif"

typedef struct {
    uint32_t recording;     /* 1 if host is actively recording GIF, 0 otherwise */
    uint32_t frame_count;   /* Number of frames captured so far */
    uint32_t max_frames;    /* Max frames to record (0 = unlimited / until exit) */
    uint32_t delay_cs;      /* Frame delay in centiseconds (1/100s, e.g. 2 = 50 FPS) */
    uint32_t save_trigger;  /* ROM can set to 1 to request capturing a frame / flush */
} wgif_t;

#endif
