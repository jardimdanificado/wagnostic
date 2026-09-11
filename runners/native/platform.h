#ifndef WAGNOSTIC_PLATFORM_H
#define WAGNOSTIC_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32)
#include <windows.h>
typedef HANDLE wthread_t;
typedef CRITICAL_SECTION wmutex_t;
typedef CONDITION_VARIABLE wcond_t;
#else
#include <pthread.h>
#include <time.h>
#include <unistd.h>
typedef pthread_t wthread_t;
typedef pthread_mutex_t wmutex_t;
typedef pthread_cond_t wcond_t;
#endif

typedef void *(*wthread_fn)(void *arg);

/* Threading API */
int wthread_create(wthread_t *thread, wthread_fn fn, void *arg);
int wthread_join(wthread_t thread, void **retval);
void wthread_sleep_ms(uint32_t ms);

/* Mutex API */
int wmutex_init(wmutex_t *mutex);
int wmutex_lock(wmutex_t *mutex);
int wmutex_unlock(wmutex_t *mutex);
void wmutex_destroy(wmutex_t *mutex);

/* Condition Variable API */
int wcond_init(wcond_t *cond);
int wcond_wait(wcond_t *cond, wmutex_t *mutex);
int wcond_timedwait(wcond_t *cond, wmutex_t *mutex, uint64_t deadline_ms);
int wcond_signal(wcond_t *cond);
int wcond_broadcast(wcond_t *cond);
void wcond_destroy(wcond_t *cond);

/* Monotonic Global Clock (milliseconds) */
uint64_t wclock_ms(void);

#endif /* WAGNOSTIC_PLATFORM_H */
