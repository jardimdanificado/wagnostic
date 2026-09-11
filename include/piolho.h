#ifndef PIOLHO_H
#define PIOLHO_H

#include <stdint.h>
#include <stddef.h>

#define PIOLHO_VERSION 2

/* Update Status Codes */
#define UPDATE_OK       0
#define UPDATE_EXIT     1
#define UPDATE_ERROR   -1

/* Legacy Update Aliases */
#define WUPDATE_OK      UPDATE_OK
#define WUPDATE_EXIT    UPDATE_EXIT
#define WUPDATE_ERROR   UPDATE_ERROR

/* IPC Status Codes */
#define IPC_OK          1
#define IPC_TIMEOUT     0
#define IPC_ERROR      -1
#define IPC_TARGET     -2
#define IPC_PARAM      -3
#define IPC_SIZE       -4
#define IPC_SHUTDOWN   -5
#define IPC_STATE      -6

/* Legacy IPC Aliases */
#define WIPC_OK         IPC_OK
#define WIPC_TIMEOUT    IPC_TIMEOUT
#define WIPC_ERROR      IPC_ERROR
#define WIPC_TARGET     IPC_TARGET
#define WIPC_PARAM      IPC_PARAM
#define WIPC_SIZE       IPC_SIZE
#define WIPC_SHUTDOWN   IPC_SHUTDOWN
#define WIPC_STATE      IPC_STATE

/* Wildcard Target for hear (receives from any sender) */
#define ANY             ((const char*)0)
#define HEAR_ANY        ((const char*)0)
#define PIOLHO_ANY      ((const char*)0)
#define WIPC_ANY        ((const char*)0)

#ifdef __cplusplus
extern "C" {
#endif

/* Capability / Extension Dispatcher */
void *use(const char *name);

/* Rendezvous IPC */
int32_t hear(const char *target, void *data, int32_t size, int32_t timeout);
int32_t tell(const char *target, const void *data, int32_t size, int32_t timeout);

/* ROM Lifecycle */
int32_t setup(void);
int32_t update(void);
int32_t shutdown(void);

/* Legacy Function Aliases */
#define wextension(name)              use(name)
#define wask(target, data, size, to)  hear(target, data, size, to)
#define wtell(target, data, size, to) tell(target, data, size, to)
#define winit()                       setup()
#define wupdate()                     update()
#define wexit()                       shutdown()

#ifdef __cplusplus
}
#endif

#endif /* PIOLHO_H */
