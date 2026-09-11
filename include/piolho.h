#ifndef PIOLHO_H
#define PIOLHO_H

#include <stdint.h>
#include <stddef.h>

#define PIOLHO_VERSION 2

/* Unified Status & Return Codes */
#define OK               0   /* Success */
#define DONE             1   /* Finished / Clean Exit / End of stream */
#define EXIT             1   /* Alias for DONE */
#define TIMEOUT          2   /* Timed out before match/rendezvous */

#define ERROR           -1   /* Generic error */
#define ERROR_TARGET    -2   /* Target peer/worker not found */
#define ERROR_PARAM     -3   /* Invalid parameter or memory bounds */
#define ERROR_SIZE      -4   /* Message payload exceeds buffer size */
#define ERROR_CLOSED    -5   /* Host, worker, or channel is closed */
#define ERROR_STATE     -6   /* Invalid runtime state / reentrancy */

/* Backwards Compatibility Aliases */
#define ERR             ERROR
#define ERR_TARGET      ERROR_TARGET
#define ERR_PARAM       ERROR_PARAM
#define ERR_SIZE        ERROR_SIZE
#define ERR_CLOSED      ERROR_CLOSED
#define ERR_STATE       ERROR_STATE
#define UPDATE_OK       OK
#define UPDATE_EXIT     EXIT
#define UPDATE_ERROR    ERROR
#define IPC_OK          OK
#define IPC_TIMEOUT     TIMEOUT
#define IPC_ERROR       ERROR
#define IPC_TARGET      ERROR_TARGET
#define IPC_PARAM       ERROR_PARAM
#define IPC_SIZE        ERROR_SIZE
#define ERROR_SHUTDOWN  ERROR_CLOSED
#define IPC_SHUTDOWN    ERROR_CLOSED

/* Wildcard Target for hear (receives from any sender) */
#define ANY             ((const char*)0)
#define HEAR_ANY        ((const char*)0)

#ifdef __cplusplus
extern "C" {
#endif

/* Capability / Extension Dispatcher */
void *use(const char *name);

/* Rendezvous IPC */
int32_t tell(const char *target, const void *data, int32_t size, int32_t timeout);
int32_t hear(const char *target, void *data, int32_t size, int32_t timeout);

/* Primary Module Entry Point */
int32_t update(void);

#ifdef __cplusplus
}
#endif

#endif /* PIOLHO_H */
