
#include <trace/hooks/binder.h>
#include <drivers/android/binder_alloc.h>
#include <drivers/android/binder_internal.h>


void binder_buffer_watcher(void *ignore, size_t size, size_t *free_async_space, 
							int is_async,  bool *should_fail);
