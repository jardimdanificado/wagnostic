#include "ipc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef enum {
    WIPC_OP_NONE = 0,
    WIPC_OP_ASK,
    WIPC_OP_TELL
} WIpcOpType;

typedef struct WIpcOp {
    uint32_t worker_id;
    char caller_name[WIPC_MAX_NAME_LEN];
    char target_name[WIPC_MAX_NAME_LEN];
    uint32_t data_ptr;
    int32_t size;
    WIpcOpType op_type;

    int32_t status;
    bool active;
    bool completed;
    wcond_t cond;
} WIpcOp;

static wmutex_t g_ipc_mutex;
static WIpcWorkerInfo g_workers[WIPC_MAX_WORKERS];
static WIpcOp g_op_pool[WIPC_MAX_WORKERS];
static bool g_ipc_initialized = false;
static bool g_ipc_shutdown_flag = false;

void wipc_init(void) {
    if (g_ipc_initialized) return;
    wmutex_init(&g_ipc_mutex);
    memset(g_workers, 0, sizeof(g_workers));
    memset(g_op_pool, 0, sizeof(g_op_pool));
    for (int i = 0; i < WIPC_MAX_WORKERS; i++) {
        wcond_init(&g_op_pool[i].cond);
    }
    g_ipc_shutdown_flag = false;
    g_ipc_initialized = true;
}

void wipc_shutdown(void) {
    if (!g_ipc_initialized) return;
    wmutex_lock(&g_ipc_mutex);
    g_ipc_shutdown_flag = true;

    for (int i = 0; i < WIPC_MAX_WORKERS; i++) {
        if (g_op_pool[i].active && !g_op_pool[i].completed) {
            g_op_pool[i].status = WIPC_SHUTDOWN;
            g_op_pool[i].completed = true;
            wcond_broadcast(&g_op_pool[i].cond);
        }
    }
    wmutex_unlock(&g_ipc_mutex);
}

void wipc_cleanup(void) {
    if (!g_ipc_initialized) return;
    wipc_shutdown();
    for (int i = 0; i < WIPC_MAX_WORKERS; i++) {
        wcond_destroy(&g_op_pool[i].cond);
    }
    wmutex_destroy(&g_ipc_mutex);
    g_ipc_initialized = false;
}

int wipc_register_worker(uint32_t id, const char *name, uint8_t *mem, uint32_t mem_len) {
    wmutex_lock(&g_ipc_mutex);
    for (int i = 0; i < WIPC_MAX_WORKERS; i++) {
        if (!g_workers[i].active) {
            g_workers[i].id = id;
            strncpy(g_workers[i].name, name, WIPC_MAX_NAME_LEN - 1);
            g_workers[i].name[WIPC_MAX_NAME_LEN - 1] = '\0';
            g_workers[i].mem = mem;
            g_workers[i].mem_len = mem_len;
            g_workers[i].active = true;
            wmutex_unlock(&g_ipc_mutex);
            return 0;
        }
    }
    wmutex_unlock(&g_ipc_mutex);
    return -1;
}

void wipc_unregister_worker(uint32_t id) {
    if (!g_ipc_initialized) return;
    wmutex_lock(&g_ipc_mutex);
    char exited_name[WIPC_MAX_NAME_LEN] = {0};
    for (int i = 0; i < WIPC_MAX_WORKERS; i++) {
        if (g_workers[i].active && g_workers[i].id == id) {
            strncpy(exited_name, g_workers[i].name, WIPC_MAX_NAME_LEN - 1);
            g_workers[i].active = false;
            break;
        }
    }

    /* Wake any waiters involving this worker */
    for (int i = 0; i < WIPC_MAX_WORKERS; i++) {
        if (g_op_pool[i].active && !g_op_pool[i].completed) {
            if (g_op_pool[i].worker_id == id || (exited_name[0] && strcmp(g_op_pool[i].target_name, exited_name) == 0)) {
                g_op_pool[i].status = WIPC_TARGET;
                g_op_pool[i].completed = true;
                wcond_broadcast(&g_op_pool[i].cond);
            }
        }
    }
    wmutex_unlock(&g_ipc_mutex);
}

void wipc_update_worker_memory(uint32_t id, uint8_t *mem, uint32_t mem_len) {
    wmutex_lock(&g_ipc_mutex);
    for (int i = 0; i < WIPC_MAX_WORKERS; i++) {
        if (g_workers[i].active && g_workers[i].id == id) {
            g_workers[i].mem = mem;
            g_workers[i].mem_len = mem_len;
            break;
        }
    }
    wmutex_unlock(&g_ipc_mutex);
}

static WIpcWorkerInfo *find_worker_by_id_locked(uint32_t id) {
    for (int i = 0; i < WIPC_MAX_WORKERS; i++) {
        if (g_workers[i].active && g_workers[i].id == id) return &g_workers[i];
    }
    return NULL;
}

static WIpcWorkerInfo *find_worker_by_name_locked(const char *name) {
    for (int i = 0; i < WIPC_MAX_WORKERS; i++) {
        if (g_workers[i].active && strcmp(g_workers[i].name, name) == 0) return &g_workers[i];
    }
    return NULL;
}

static int32_t execute_rendezvous(uint32_t caller_id, const char *target, uint32_t data_ptr, int32_t size, int32_t timeout, WIpcOpType op_type) {
    if (!g_ipc_initialized || g_ipc_shutdown_flag) return WIPC_SHUTDOWN;
    if (size < 0) return WIPC_PARAM;
    if (timeout < -1) return WIPC_PARAM;
    if (!target || target[0] == '\0') return WIPC_PARAM;

    wmutex_lock(&g_ipc_mutex);

    WIpcWorkerInfo *caller = find_worker_by_id_locked(caller_id);
    if (!caller) {
        wmutex_unlock(&g_ipc_mutex);
        return WIPC_ERROR;
    }

    if (strcmp(caller->name, target) == 0) {
        wmutex_unlock(&g_ipc_mutex);
        return WIPC_PARAM; /* Self-communication disallowed */
    }

    WIpcWorkerInfo *tgt = find_worker_by_name_locked(target);
    if (!tgt && timeout == 0) {
        wmutex_unlock(&g_ipc_mutex);
        return WIPC_TARGET;
    }

    /* Validate caller buffer range */
    if (size > 0 && data_ptr + (uint32_t)size > caller->mem_len) {
        wmutex_unlock(&g_ipc_mutex);
        return WIPC_PARAM;
    }

    /* Check if matching opposite operation is waiting */
    WIpcOpType matching_type = (op_type == WIPC_OP_ASK) ? WIPC_OP_TELL : WIPC_OP_ASK;
    WIpcOp *match = NULL;
    for (int i = 0; i < WIPC_MAX_WORKERS; i++) {
        if (g_op_pool[i].active && !g_op_pool[i].completed &&
            g_op_pool[i].op_type == matching_type &&
            strcmp(g_op_pool[i].caller_name, target) == 0 &&
            strcmp(g_op_pool[i].target_name, caller->name) == 0) {
            match = &g_op_pool[i];
            break;
        }
    }

    if (match) {
        /* Partner found! Determine sender vs receiver */
        WIpcWorkerInfo *sender_info = (op_type == WIPC_OP_TELL) ? caller : find_worker_by_id_locked(match->worker_id);
        WIpcWorkerInfo *recv_info   = (op_type == WIPC_OP_ASK)  ? caller : find_worker_by_id_locked(match->worker_id);
        
        uint32_t send_ptr = (op_type == WIPC_OP_TELL) ? data_ptr : match->data_ptr;
        int32_t  send_sz  = (op_type == WIPC_OP_TELL) ? size     : match->size;
        uint32_t recv_ptr = (op_type == WIPC_OP_ASK)  ? data_ptr : match->data_ptr;
        int32_t  recv_sz  = (op_type == WIPC_OP_ASK)  ? size     : match->size;

        if (!sender_info || !recv_info) {
            wmutex_unlock(&g_ipc_mutex);
            return WIPC_ERROR;
        }

        /* Check size constraints */
        if (send_sz > recv_sz) {
            match->status = WIPC_SIZE;
            match->completed = true;
            wcond_signal(&match->cond);
            wmutex_unlock(&g_ipc_mutex);
            return WIPC_SIZE;
        }

        /* Check memory bounds */
        if (send_sz > 0 && (send_ptr + (uint32_t)send_sz > sender_info->mem_len || recv_ptr + (uint32_t)send_sz > recv_info->mem_len)) {
            match->status = WIPC_PARAM;
            match->completed = true;
            wcond_signal(&match->cond);
            wmutex_unlock(&g_ipc_mutex);
            return WIPC_PARAM;
        }

        /* Perform direct transfer */
        if (send_sz > 0) {
            memcpy(recv_info->mem + recv_ptr, sender_info->mem + send_ptr, (size_t)send_sz);
        }

        match->status = WIPC_OK;
        match->completed = true;
        wcond_signal(&match->cond);
        wmutex_unlock(&g_ipc_mutex);
        return WIPC_OK;
    }

    /* No partner waiting */
    if (timeout == 0) {
        wmutex_unlock(&g_ipc_mutex);
        return WIPC_TIMEOUT;
    }

    /* Register in static pool */
    uint32_t slot = (caller_id > 0) ? (caller_id - 1) % WIPC_MAX_WORKERS : 0;
    WIpcOp *op = &g_op_pool[slot];
    op->worker_id = caller_id;
    strncpy(op->caller_name, caller->name, WIPC_MAX_NAME_LEN - 1);
    strncpy(op->target_name, target, WIPC_MAX_NAME_LEN - 1);
    op->data_ptr = data_ptr;
    op->size = size;
    op->op_type = op_type;
    op->status = WIPC_TIMEOUT;
    op->completed = false;
    op->active = true;

    uint64_t deadline = (timeout > 0) ? (wclock_ms() + (uint64_t)timeout) : 0;

    while (!op->completed && !g_ipc_shutdown_flag) {
        if (timeout > 0) {
            int r = wcond_timedwait(&op->cond, &g_ipc_mutex, deadline);
            if (r != 0) break; // timeout or error
        } else {
            wcond_wait(&op->cond, &g_ipc_mutex);
        }
    }

    int32_t result = op->status;
    if (!op->completed) {
        if (g_ipc_shutdown_flag) result = WIPC_SHUTDOWN;
        else result = WIPC_TIMEOUT;
    }

    op->active = false;
    wmutex_unlock(&g_ipc_mutex);
    return result;
}

int32_t wipc_ask(uint32_t caller_id, const char *target, uint32_t data_ptr, int32_t size, int32_t timeout) {
    return execute_rendezvous(caller_id, target, data_ptr, size, timeout, WIPC_OP_ASK);
}

int32_t wipc_tell(uint32_t caller_id, const char *target, uint32_t data_ptr, int32_t size, int32_t timeout) {
    return execute_rendezvous(caller_id, target, data_ptr, size, timeout, WIPC_OP_TELL);
}
