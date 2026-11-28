/* Access to the thread storage of the Glide context (gthread.c, fxglide.h)
 * for thread_storage_test.cpp. getThreadValueFast is an inline function of
 * the internal header fxglide.h, which is C only. */

#ifndef THREAD_STORAGE_ACCESS_H
#define THREAD_STORAGE_ACCESS_H

#ifdef __cplusplus
extern "C" {
#endif

void tsaInitThreadStorage(void);
void tsaFreeThreadStorage(void);
unsigned long tsaTlsIndex(void);
void tsaSetThreadValue(void *value);
void *tsaGetThreadValueFast(void);

#ifdef __cplusplus
}
#endif

#endif
