#define _POSIX_C_SOURCE 200809L
#include "platform.h"
#include <stdlib.h>
#include <errno.h>

#if defined(_WIN32)
uint64_t wclock_ms(void) {
    return (uint64_t)GetTickCount64();
}

void wthread_sleep_ms(uint32_t ms) {
    Sleep(ms);
}

int wthread_create(wthread_t *thread, wthread_fn fn, void *arg) {
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fn, arg, 0, NULL);
    return (*thread != NULL) ? 0 : -1;
}

int wthread_join(wthread_t thread, void **retval) {
    WaitForSingleObject(thread, INFINITE);
    if (retval) GetExitCodeThread(thread, (LPDWORD)retval);
    CloseHandle(thread);
    return 0;
}

int wmutex_init(wmutex_t *mutex) {
    InitializeCriticalSection(mutex);
    return 0;
}

int wmutex_lock(wmutex_t *mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

int wmutex_unlock(wmutex_t *mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

void wmutex_destroy(wmutex_t *mutex) {
    DeleteCriticalSection(mutex);
}

int wcond_init(wcond_t *cond) {
    InitializeConditionVariable(cond);
    return 0;
}

int wcond_wait(wcond_t *cond, wmutex_t *mutex) {
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
}

int wcond_timedwait(wcond_t *cond, wmutex_t *mutex, uint64_t deadline_ms) {
    uint64_t now = wclock_ms();
    if (now >= deadline_ms) return 1; // timeout
    DWORD wait_ms = (DWORD)(deadline_ms - now);
    if (!SleepConditionVariableCS(cond, mutex, wait_ms)) {
        if (GetLastError() == ERROR_TIMEOUT) return 1;
        return -1;
    }
    return 0;
}

int wcond_signal(wcond_t *cond) {
    WakeConditionVariable(cond);
    return 0;
}

int wcond_broadcast(wcond_t *cond) {
    WakeAllConditionVariable(cond);
    return 0;
}

void wcond_destroy(wcond_t *cond) {
    (void)cond;
}

#else

uint64_t wclock_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000ULL) + ((uint64_t)ts.tv_nsec / 1000000ULL);
}

void wthread_sleep_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000ULL;
    nanosleep(&ts, NULL);
}

int wthread_create(wthread_t *thread, wthread_fn fn, void *arg) {
    return pthread_create(thread, NULL, fn, arg);
}

int wthread_join(wthread_t thread, void **retval) {
    return pthread_join(thread, retval);
}

int wmutex_init(wmutex_t *mutex) {
    return pthread_mutex_init(mutex, NULL);
}

int wmutex_lock(wmutex_t *mutex) {
    return pthread_mutex_lock(mutex);
}

int wmutex_unlock(wmutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}

void wmutex_destroy(wmutex_t *mutex) {
    pthread_mutex_destroy(mutex);
}

int wcond_init(wcond_t *cond) {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    int res = pthread_cond_init(cond, &attr);
    pthread_condattr_destroy(&attr);
    return res;
}

int wcond_wait(wcond_t *cond, wmutex_t *mutex) {
    return pthread_cond_wait(cond, mutex);
}

int wcond_timedwait(wcond_t *cond, wmutex_t *mutex, uint64_t deadline_ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(deadline_ms / 1000ULL);
    ts.tv_nsec = (long)((deadline_ms % 1000ULL) * 1000000ULL);
    int rc = pthread_cond_timedwait(cond, mutex, &ts);
    if (rc == ETIMEDOUT) return 1; // timeout
    if (rc != 0) return -1;
    return 0;
}

int wcond_signal(wcond_t *cond) {
    return pthread_cond_signal(cond);
}

int wcond_broadcast(wcond_t *cond) {
    return pthread_cond_broadcast(cond);
}

void wcond_destroy(wcond_t *cond) {
    pthread_cond_destroy(cond);
}

#endif
