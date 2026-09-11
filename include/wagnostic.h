#ifndef WAGNOSTIC_H
#define WAGNOSTIC_H

#include <stdint.h>
#include <stddef.h>

#define WAGNOSTIC_VERSION 2

#define WUPDATE_OK      0
#define WUPDATE_EXIT    1
#define WUPDATE_ERROR  -1

/* IPC Status Codes */
#define WIPC_OK          1
#define WIPC_TIMEOUT     0
#define WIPC_ERROR      -1
#define WIPC_TARGET     -2
#define WIPC_PARAM      -3
#define WIPC_SIZE       -4
#define WIPC_SHUTDOWN   -5
#define WIPC_STATE      -6

#ifdef __cplusplus
extern "C" {
#endif

/* Capability Dispatcher */
void *wextension(const char *name);

/* Rendezvous IPC */
int32_t wask(const char *target, void *data, int32_t size, int32_t timeout);
int32_t wtell(const char *target, const void *data, int32_t size, int32_t timeout);

/* ROM Lifecycle */
int32_t winit(void);
int32_t wupdate(void);
int32_t wexit(void);

#ifdef __cplusplus
}
#endif

#endif /* WAGNOSTIC_H */
