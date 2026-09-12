#ifndef WAGNOSTIC_H
#define WAGNOSTIC_H

#include <stdint.h>
#include <stddef.h>

#define WAGNOSTIC_VERSION 2

#define UPDATE_OK      0
#define UPDATE_EXIT    1
#define UPDATE_ERROR  -1

#define OK             UPDATE_OK
#define DONE           UPDATE_EXIT
#define ERROR          UPDATE_ERROR

#ifdef __cplusplus
extern "C" {
#endif

void *ask(const char *name);

int32_t update(void);

#ifdef __cplusplus
}
#endif

#endif /* WAGNOSTIC_H */
