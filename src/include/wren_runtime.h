#ifndef wren_runtime_h
#define wren_runtime_h

#include <wren.h>

typedef WrenForeignMethodFn (*WrtPluginInitFunc)(int handle);

void wrt_init();
WrenVM* wrt_new_wren_vm(bool isMain);
void wrt_bind_class(const char* name, WrenForeignMethodFn allocator, WrenFinalizerFn finalizer);
void wrt_bind_method(const char* name, WrenForeignMethodFn func);
void wrt_set_plugin_data(WrenVM* vm, int handle, void* value);
void* wrt_get_plugin_data(WrenVM* vm, int handle);
void wrt_wren_update_callback(WrenForeignMethodFn fn);
void wrt_call_update_callbacks(WrenVM* vm);
void wrt_register_plugin(const char* name, WrtPluginInitFunc initfunc);

#endif