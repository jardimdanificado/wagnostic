#ifndef WAGNOSTIC_WORKER_H
#define WAGNOSTIC_WORKER_H

#include <stdint.h>
#include <stdbool.h>
#include "wasm3.h"
#include "m3_env.h"
#include "platform.h"
#include "framebuffer.h"
#include "clock.h"
#include "keyboard.h"
#include "mouse.h"
#include "gamepad.h"
#include "gif.h"
#include "logger.h"

typedef struct WWorker {
    uint32_t id;
    char name[64];
    char wasm_path[256];

    uint8_t *wasm_bytes;
    size_t wasm_size;

    IM3Environment env;
    IM3Runtime runtime;
    IM3Module module;

    IM3Function f_winit;
    IM3Function f_wupdate;
    IM3Function f_wexit;

    wthread_t thread;
    bool thread_started;
    volatile bool running;
    int exit_code;
    volatile uint64_t frame_count;

    /* WASM Memory & Arena */
    uint8_t *mem;
    uint32_t mem_len;
    uint32_t arena;

    /* Extensions state */
    uint32_t fb_ptr;
    uint32_t clock_ptr;
    uint32_t keyboard_ptr;
    uint32_t mouse_ptr;
    uint32_t gamepad_ptr;
    uint32_t gif_ptr;
    uint32_t logger_ptr;
    uint32_t logger_buf_ptr;

    uint32_t default_fb_ptr;
} WWorker;

WWorker *worker_create(uint32_t id, const char *name, const char *wasm_path, uint8_t *wasm_bytes, size_t wasm_size);
int worker_start(WWorker *w);
int worker_join(WWorker *w);
void worker_stop(WWorker *w);
void worker_destroy(WWorker *w);

#endif /* WAGNOSTIC_WORKER_H */
