#ifndef wren_runtime_h
#define wren_runtime_h

#include <wren.h>

typedef WrenForeignMethodFn (*WrtPluginInitFunc)(int handle);

void wrt_init(const char* root);
WrenVM* wrt_new_wren_vm(bool isMain);
void wrt_bind_class(const char* name, WrenForeignMethodFn allocator, WrenFinalizerFn finalizer);
void wrt_bind_method(const char* name, WrenForeignMethodFn func);
void wrt_set_plugin_data(WrenVM* vm, int handle, void* value);
void* wrt_get_plugin_data(WrenVM* vm, int handle);
void wrt_wren_update_callback(WrenForeignMethodFn fn);
void wrt_call_update_callbacks(WrenVM* vm);
void wrt_register_plugin(const char* name, WrtPluginInitFunc initfunc);
void wrt_run_main(WrenVM* vm, const char* module);

#define WREN_METHOD(NAME) static void NAME(WrenVM* vm)
#define WREN_CONSTRUCTOR(NAME) static void NAME(WrenVM* vm)
#define WREN_DESTRUCTOR(NAME) static void NAME(void* data)

#endif