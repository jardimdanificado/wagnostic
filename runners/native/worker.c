#define _POSIX_C_SOURCE 200809L
#include "worker.h"
#include "ipc.h"
#include "m3_api_libc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void refresh_worker_memory(WWorker *w) {
    if (!w || !w->runtime) return;
    w->mem = m3_GetMemory(w->runtime, &w->mem_len, 0);
    wipc_update_worker_memory(w->id, w->mem, w->mem_len);
}

static uint32_t worker_alloc(WWorker *w, uint32_t size, uint32_t align) {
    refresh_worker_memory(w);
    if (w->arena == 0) {
        if (w->mem_len >= 2097152) {
            w->arena = w->mem_len - 1400000;
        } else if (w->mem_len >= 1048576) {
            w->arena = w->mem_len - 400000;
        } else {
            w->arena = 0x8000;
        }
    }
    if (align > 1) {
        w->arena = (w->arena + align - 1) & ~(align - 1);
    }
    uint32_t ptr = w->arena;
    w->arena += size;
    return ptr;
}

/* ── Host Import: wextension ── */
m3ApiRawFunction(worker_host_wextension) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, name_ptr);

    WWorker *w = (WWorker*)m3_GetUserData(runtime);
    if (!w) m3ApiReturn(0);

    refresh_worker_memory(w);
    if (!w->mem || name_ptr >= w->mem_len) {
        m3ApiReturn(0);
    }

    const char *name = (const char*)(w->mem + name_ptr);

    /* 1. Framebuffer: std:framebuffer */
    if (strcmp(name, WFRAMEBUFFER_EXTENSION) == 0 || strcmp(name, "framebuffer") == 0 ||
        strcmp(name, WSURFACE_EXTENSION) == 0 || strcmp(name, "surface") == 0) {
        if (w->fb_ptr == 0) {
            w->fb_ptr = worker_alloc(w, sizeof(wframebuffer_t), 4);
            w->default_fb_ptr = worker_alloc(w, 320 * 240 * 4, 4);
            wframebuffer_t *fb = (wframebuffer_t*)(w->mem + w->fb_ptr);
            fb->width = 320;
            fb->height = 240;
            fb->pixels = w->default_fb_ptr;
        }
        m3ApiReturn(w->fb_ptr);
    }

    /* 2. Clock: std:clock */
    if (strcmp(name, WCLOCK_EXTENSION) == 0 || strcmp(name, "clock") == 0) {
        if (w->clock_ptr == 0) {
            w->clock_ptr = worker_alloc(w, sizeof(wclock_t), 8);
            wclock_t *clk = (wclock_t*)(w->mem + w->clock_ptr);
            clk->ticks = 0;
            clk->frequency = 1000;
            clk->delta = 1.0f / 60.0f;
        }
        m3ApiReturn(w->clock_ptr);
    }

    /* 3. Keyboard: std:keyboard */
    if (strcmp(name, WKEYBOARD_EXTENSION) == 0 || strcmp(name, "keyboard") == 0) {
        if (w->keyboard_ptr == 0) {
            w->keyboard_ptr = worker_alloc(w, sizeof(wkeyboard_t), 4);
            wkeyboard_t *kb = (wkeyboard_t*)(w->mem + w->keyboard_ptr);
            memset(kb, 0, sizeof(wkeyboard_t));
        }
        m3ApiReturn(w->keyboard_ptr);
    }

    /* 4. Mouse: std:mouse */
    if (strcmp(name, WMOUSE_EXTENSION) == 0 || strcmp(name, "mouse") == 0) {
        if (w->mouse_ptr == 0) {
            w->mouse_ptr = worker_alloc(w, sizeof(wmouse_t), 4);
            wmouse_t *mouse = (wmouse_t*)(w->mem + w->mouse_ptr);
            memset(mouse, 0, sizeof(wmouse_t));
        }
        m3ApiReturn(w->mouse_ptr);
    }

    /* 5. Gamepad: std:gamepad */
    if (strcmp(name, WGAMEPAD_EXTENSION) == 0 || strcmp(name, "gamepad") == 0) {
        if (w->gamepad_ptr == 0) {
            w->gamepad_ptr = worker_alloc(w, sizeof(wgamepad_t), 4);
            wgamepad_t *gp = (wgamepad_t*)(w->mem + w->gamepad_ptr);
            memset(gp, 0, sizeof(wgamepad_t));
        }
        m3ApiReturn(w->gamepad_ptr);
    }

    /* 6. GIF Recording: std:gif */
    if (strcmp(name, WGIF_EXTENSION) == 0 || strcmp(name, "gif") == 0) {
        if (w->gif_ptr == 0) {
            w->gif_ptr = worker_alloc(w, sizeof(wgif_t), 4);
            wgif_t *g = (wgif_t*)(w->mem + w->gif_ptr);
            g->recording = 0;
            g->frame_count = 0;
            g->max_frames = 0;
            g->delay_cs = 2;
            g->save_trigger = 0;
        }
        m3ApiReturn(w->gif_ptr);
    }

    /* 7. Logger: logger */
    if (strcmp(name, "logger") == 0) {
        if (w->logger_ptr == 0) {
            w->logger_ptr = worker_alloc(w, sizeof(wlogger_t), 4);
            w->logger_buf_ptr = worker_alloc(w, 1024, 4);
            wlogger_t *log = (wlogger_t*)(w->mem + w->logger_ptr);
            log->buffer = w->logger_buf_ptr;
            log->capacity = 1024;
            log->length = 0;
        }
        m3ApiReturn(w->logger_ptr);
    }

    m3ApiReturn(0);
}

/* ── Host Import: wask ── */
m3ApiRawFunction(worker_host_wask) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, target_ptr);
    m3ApiGetArg(uint32_t, data_ptr);
    m3ApiGetArg(int32_t, size);
    m3ApiGetArg(int32_t, timeout);

    WWorker *w = (WWorker*)m3_GetUserData(runtime);
    if (!w) m3ApiReturn(WIPC_ERROR);

    refresh_worker_memory(w);
    if (!w->mem || target_ptr >= w->mem_len) {
        m3ApiReturn(WIPC_PARAM);
    }

    const char *target = (const char*)(w->mem + target_ptr);
    int32_t res = wipc_ask(w->id, target, data_ptr, size, timeout);
    m3ApiReturn(res);
}

/* ── Host Import: wtell ── */
m3ApiRawFunction(worker_host_wtell) {
    m3ApiReturnType(int32_t);
    m3ApiGetArg(uint32_t, target_ptr);
    m3ApiGetArg(uint32_t, data_ptr);
    m3ApiGetArg(int32_t, size);
    m3ApiGetArg(int32_t, timeout);

    WWorker *w = (WWorker*)m3_GetUserData(runtime);
    if (!w) m3ApiReturn(WIPC_ERROR);

    refresh_worker_memory(w);
    if (!w->mem || target_ptr >= w->mem_len) {
        m3ApiReturn(WIPC_PARAM);
    }

    const char *target = (const char*)(w->mem + target_ptr);
    int32_t res = wipc_tell(w->id, target, data_ptr, size, timeout);
    m3ApiReturn(res);
}

static void *worker_thread_main(void *arg) {
    WWorker *w = (WWorker*)arg;
    if (!w) return NULL;

    /* 1. Optional winit() */
    if (w->f_winit) {
        M3Result res = m3_CallV(w->f_winit);
        if (res) {
            fprintf(stderr, "[Worker %s] Error in winit(): %s\n", w->name, res);
            w->exit_code = -1;
            w->running = false;
            wipc_unregister_worker(w->id);
            return NULL;
        }
    }

    /* 2. Main execution loop: wupdate() */
    uint64_t start_ms = wclock_ms();
    uint64_t last_ms = start_ms;

    while (w->running) {
        uint64_t now_ms = wclock_ms();
        double elapsed_ms = (double)(now_ms - start_ms);
        double dt = (double)(now_ms - last_ms) / 1000.0;
        last_ms = now_ms;

        /* Update clock extension */
        if (w->clock_ptr && w->clock_ptr + sizeof(wclock_t) <= w->mem_len) {
            wclock_t *clk = (wclock_t*)(w->mem + w->clock_ptr);
            clk->ticks = (uint64_t)elapsed_ms;
            clk->delta = (float)dt;
        }

        /* Flush logger */
        if (w->logger_ptr && w->logger_ptr + sizeof(wlogger_t) <= w->mem_len) {
            wlogger_t *log = (wlogger_t*)(w->mem + w->logger_ptr);
            uint32_t len = log->length;
            if (len > 0 && w->logger_buf_ptr && w->logger_buf_ptr + len <= w->mem_len) {
                char log_str[1024];
                uint32_t cpy_len = len < 1023 ? len : 1023;
                memcpy(log_str, w->mem + w->logger_buf_ptr, cpy_len);
                log_str[cpy_len] = '\0';
                printf("[%s Log] %s\n", w->name, log_str);
                log->length = 0;
            }
        }

        if (w->f_wupdate) {
            M3Result res = m3_CallV(w->f_wupdate);
            if (res) {
                fprintf(stderr, "[Worker %s] Error in wupdate(): %s\n", w->name, res);
                w->exit_code = -1;
                break;
            }

            int32_t status = 0;
            m3_GetResultsV(w->f_wupdate, &status);
            w->frame_count++;

            if (status == WUPDATE_EXIT) {
                w->exit_code = 0;
                break;
            }
            if (status < 0) {
                w->exit_code = status;
                break;
            }
        } else {
            break;
        }
    }

    /* 3. Optional wexit() */
    if (w->f_wexit) {
        m3_CallV(w->f_wexit);
    }

    wipc_unregister_worker(w->id);
    w->running = false;
    return NULL;
}

WWorker *worker_create(uint32_t id, const char *name, const char *wasm_path, uint8_t *wasm_bytes, size_t wasm_size) {
    WWorker *w = (WWorker*)calloc(1, sizeof(WWorker));
    if (!w) return NULL;

    w->id = id;
    strncpy(w->name, name ? name : "worker", sizeof(w->name) - 1);
    if (wasm_path) strncpy(w->wasm_path, wasm_path, sizeof(w->wasm_path) - 1);

    w->wasm_bytes = (uint8_t*)malloc(wasm_size);
    if (!w->wasm_bytes) { free(w); return NULL; }
    memcpy(w->wasm_bytes, wasm_bytes, wasm_size);
    w->wasm_size = wasm_size;

    w->env = m3_NewEnvironment();
    if (!w->env) { free(w->wasm_bytes); free(w); return NULL; }

    w->runtime = m3_NewRuntime(w->env, 64 * 1024, w);
    if (!w->runtime) { m3_FreeEnvironment(w->env); free(w->wasm_bytes); free(w); return NULL; }

    M3Result res = m3_ParseModule(w->env, &w->module, w->wasm_bytes, w->wasm_size);
    if (res) {
        fprintf(stderr, "Error: ParseModule failed for %s: %s\n", w->name, res);
        worker_destroy(w);
        return NULL;
    }

    res = m3_LoadModule(w->runtime, w->module);
    if (res) {
        fprintf(stderr, "Error: LoadModule failed for %s: %s\n", w->name, res);
        worker_destroy(w);
        return NULL;
    }

    /* Link core imports */
    m3_LinkRawFunction(w->module, "env", "wextension", "i(i)", &worker_host_wextension);
    m3_LinkRawFunction(w->module, "env", "wask", "i(iiii)", &worker_host_wask);
    m3_LinkRawFunction(w->module, "env", "wtell", "i(iiii)", &worker_host_wtell);

    m3_LinkLibC(w->module);

    /* Look up exports */
    m3_FindFunction(&w->f_winit, w->runtime, "winit");
    m3_FindFunction(&w->f_wupdate, w->runtime, "wupdate");
    m3_FindFunction(&w->f_wexit, w->runtime, "wexit");

    refresh_worker_memory(w);
    wipc_register_worker(w->id, w->name, w->mem, w->mem_len);
    w->running = false;
    w->thread_started = false;

    return w;
}

int worker_start(WWorker *w) {
    if (!w) return -1;
    w->running = true;
    int res = wthread_create(&w->thread, worker_thread_main, w);
    if (res == 0) {
        w->thread_started = true;
    }
    return res;
}

int worker_join(WWorker *w) {
    if (!w || !w->thread_started) return 0;
    w->thread_started = false;
    void *retval = NULL;
    return wthread_join(w->thread, &retval);
}

void worker_stop(WWorker *w) {
    if (!w) return;
    w->running = false;
}

void worker_destroy(WWorker *w) {
    if (!w) return;
    worker_stop(w);
    worker_join(w);

    if (w->runtime) m3_FreeRuntime(w->runtime);
    if (w->env) m3_FreeEnvironment(w->env);
    if (w->wasm_bytes) free(w->wasm_bytes);
    free(w);
}
