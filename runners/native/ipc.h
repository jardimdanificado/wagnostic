#ifndef WAGNOSTIC_IPC_H
#define WAGNOSTIC_IPC_H

#include <stdint.h>
#include <stdbool.h>
#include "platform.h"
#include "wagnostic.h"

#define WIPC_MAX_WORKERS 64
#define WIPC_MAX_NAME_LEN 64

typedef struct {
    uint32_t id;
    char name[WIPC_MAX_NAME_LEN];
    uint8_t *mem;
    uint32_t mem_len;
    bool active;
} WIpcWorkerInfo;

void wipc_init(void);
void wipc_shutdown(void);
void wipc_cleanup(void);

int wipc_register_worker(uint32_t id, const char *name, uint8_t *mem, uint32_t mem_len);
void wipc_unregister_worker(uint32_t id);
void wipc_update_worker_memory(uint32_t id, uint8_t *mem, uint32_t mem_len);

int32_t wipc_ask(uint32_t caller_id, const char *target, uint32_t data_ptr, int32_t size, int32_t timeout);
int32_t wipc_tell(uint32_t caller_id, const char *target, uint32_t data_ptr, int32_t size, int32_t timeout);

#endif /* WAGNOSTIC_IPC_H */
