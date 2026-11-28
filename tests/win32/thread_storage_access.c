#include <3dfx.h>
#include <glidesys.h>
#include <glide.h>

#include "fxglide.h"

#include "thread_storage_access.h"

void tsaInitThreadStorage(void)
{
  initThreadStorage();
}

void tsaFreeThreadStorage(void)
{
  freeThreadStorage();
}

unsigned long tsaTlsIndex(void)
{
  return _GlideRoot.tlsIndex;
}

void tsaSetThreadValue(void *value)
{
  setThreadValue((AnyPtr)value);
}

void *tsaGetThreadValueFast(void)
{
  return (void *)getThreadValueFast();
}
