#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

#include <wren.h>

#include <wren_runtime.h>

// CAUTION: Do this only once
#define STB_DS_IMPLEMENTATION
#include <stb_ds.h>

#include "os_call.h"
#include "mutex.h"
#include "modules.h"

MUTEX mutex;
const char* moduleRoot;

typedef struct {
  char* key;
  void* value;
} WrenPluginData;

typedef struct {
  bool isMainThread;
  int numPluginData;
  void** pluginData; 
} WrenUserData;

void wrt_set_plugin_data(WrenVM* vm, int handle, void* value){
  WrenUserData* ud = (WrenUserData*)wrenGetUserData(vm);
  if(ud->numPluginData < handle){
    ud->pluginData = ud->numPluginData > 0 ? realloc(ud->pluginData, handle*sizeof(void*)) : malloc(handle*sizeof(void*));
    ud->numPluginData = handle;
  }
  ud->pluginData[handle-1] = value;
}

void* wrt_get_plugin_data(WrenVM* vm, int handle){
  WrenUserData* ud = (WrenUserData*)wrenGetUserData(vm);
  if(ud->numPluginData < handle){
    return NULL;
  }
  return ud->pluginData[handle- 1];
}

static const void error_fn(WrenVM *vm, WrenErrorType type, const char *module, int line, const char *message)
{  
  printf("Wren-Error in module '%s' line %i: %s\n", module, line, message);
}

static const void write_fn(WrenVM *vm, const char *text)
{
  printf("%s", text);
}

typedef struct {
  char * key;
  WrenForeignMethodFn value;
} Binding;

typedef struct {
  char* key;
  WrenForeignClassMethods value;
} ClassBinding;

typedef struct {
  void* wrenInitFunc;
} BinaryModuleData;

typedef struct {
  char* key;
  BinaryModuleData value;
} BinaryModule;

static Binding* bindings = NULL;
static ClassBinding* classBindings = NULL;
static BinaryModule* binaryModules = NULL;

static char* getMethodName(const char* module, 
  const char* className, 
  bool isStatic, 
  const char* signature)
{
  int length = strlen(module) + strlen(className) + strlen(signature) + 3;
  char *str = (char*)malloc(length);
  sprintf(str, "%s.%s.%s", module, className, signature);
  return str;
}

static char* getClassName(const char* module, 
  const char* className)
{
  int length = strlen(module) + strlen(className) + 2;
  char *str = (char*)malloc(length);
  sprintf(str, "%s.%s", module, className);
  return str;
}

static WrenForeignMethodFn bind_method_fn( 
  WrenVM* vm, 
  const char* module, 
  const char* className, 
  bool isStatic, 
  const char* signature) 
{
  if(strcmp(module, "random") == 0 || strcmp(module, "meta") == 0){
    return NULL;
  }
  char* fullName = getMethodName(module, className, isStatic, signature);
  WrenForeignMethodFn func = shget(bindings, fullName);
  free(fullName);
  return func;
}

WrenForeignClassMethods bind_class_fn( 
  WrenVM* vm, 
  const char* module, 
  const char* className)
{
  if(strcmp(module, "random") == 0 || strcmp(module, "meta") == 0){
    WrenForeignClassMethods wfcm ={0};
    return wfcm;
  }

  char* fullName = getClassName(module, className);
  
  int index = shgeti(classBindings, fullName);
  free(fullName);
  if(index == -1){
    WrenForeignClassMethods wfcm ={0};
    return wfcm;  
  }
  else {
    return classBindings[index].value;
  }
}



void wrt_bind_method(const char* name, WrenForeignMethodFn func){
  shput(bindings, name, func);
}

void wrt_bind_class(const char* name, WrenForeignMethodFn allocator, WrenFinalizerFn finalizer){
  WrenForeignClassMethods methods = {
    allocator = allocator,
    finalizer = finalizer
  };
  shput(classBindings, name, methods);
}

struct WrenCallbackNode {
  WrenForeignMethodFn callback;
  struct WrenCallbackNode* next;
};
typedef struct WrenCallbackNode WrenCallbackNode;
typedef struct {
  WrenCallbackNode* start;
  WrenCallbackNode* end;
}  WrenCallbackList;

static WrenCallbackList updateCallbacks;

static void callbacks_push(WrenCallbackList* list, WrenCallbackNode* node){
  if(list->start == NULL && list->end == NULL){
      list->start = node;
      list->end = node;
  } else {
    list->end->next = node;
  }
}

void wrt_wren_update_callback(WrenForeignMethodFn fn){
  WrenCallbackNode* cb = malloc(sizeof(WrenCallbackNode));
  cb->callback = fn;
  cb->next = NULL;

  callbacks_push(&updateCallbacks, cb);
}

void wrt_call_update_callbacks(WrenVM* vm) {
  while(updateCallbacks.start != NULL){
    WrenCallbackNode* current = updateCallbacks.start;
    WrenCallbackNode* prev = NULL;
    while(current != NULL){
      current->callback(vm);
      if(!wrenGetSlotBool(vm, 0)){
        if(current == updateCallbacks.end){
          updateCallbacks.end = prev;
        }      
        if(prev == NULL){
          updateCallbacks.start = current->next;        
        } else {
          prev->next = current->next;
        }
        WrenCallbackNode* remove = current;
        current = current->next;
        free(remove);
      }
    }
  }
}

static int plugin_id = 1;

#define LoadPluginAssert(assert, msg) if(!(assert)){ puts(msg); goto DONE; }

static void register_plugin(const char* name, WrtPluginInitFunc init){
  printf("Register Plugin %s\n", name);
  BinaryModuleData md;
  md.wrenInitFunc = init(plugin_id++);
  shput(binaryModules, name, md);
}

void wrt_register_plugin(const char* name, WrtPluginInitFunc init){
  MUTEX_LOCK(&mutex);
  printf("Load static binary module '%s'\n", name);
  register_plugin(name, init);
  MUTEX_UNLOCK(&mutex);
}

static void load_plugin(WrenVM* vm, const char * pluginname, const char * dllname){
  MUTEX_LOCK(&mutex);
  
  char namebuffer[1024];
  
  int len = strlen(dllname);
  char* name = malloc(sizeof(char)*len+1);
  strcpy(name, dllname);

  int index = shgeti(binaryModules, pluginname);
  if(index == -1){
    printf("Load dynamic binary module '%s'\n", pluginname);
    void* handle = wrt_dlopen(name);
    LoadPluginAssert(handle != NULL, "Could not open binary plugin");

    strcpy(namebuffer, "wrt_plugin_init_");
    strcat(namebuffer, pluginname);
    WrenForeignMethodFn (*initFunc)(int handle) = wrt_dlsym(handle, namebuffer);
    LoadPluginAssert(initFunc != NULL, "Did not find init entry point in binary plugin");

    register_plugin(pluginname, initFunc);
    index = shgeti(binaryModules, pluginname);
  }
  else {
    free(name);
  }

  void* initFunc = binaryModules[index].value.wrenInitFunc;
  if(initFunc != NULL){
    void (*wrenInitFunc)(WrenVM*) = initFunc;
    wrenInitFunc(vm);
  }

  DONE:
  MUTEX_UNLOCK(&mutex);
}

static WrenLoadModuleResult load_module_fn(WrenVM* vm, const char* name){
  WrenLoadModuleResult result = {0};
  const char* script = NULL;

  if(strcmp(name, "random") == 0 || strcmp(name, "meta") == 0){
    return result;
  }

  if(wrt_is_file_module(name)){
    if(wrt_file_exists(name)){
      script = wrt_read_file(name);
    }
  } else {
    const char* module_path = wrt_resolve_installed_module(&moduleRoot, 1, name);
    if(module_path == NULL) return result;
    printf("Resolved installed module at %s\n", module_path);
    if(wrt_file_exists(module_path)){
      script = wrt_read_file(module_path);
    }
    
    const char* binary_path = wrt_resolve_binary_module(module_path);
    free((void*)module_path);
    printf("Load binary %s\n", binary_path);
    if(wrt_file_exists(binary_path)){
      load_plugin(vm, name, binary_path);
    }
    free((void*)binary_path);
  } 
  result.source = script;
  return result;
}

static const char* resolve_module_fn(WrenVM* vm, const char* importer, const char* name){

  if(strcmp(name, "random") == 0 || strcmp(name, "meta") == 0){
    return name;
  }

  const char* resolved;
  if(wrt_is_file_module(name)){
    resolved = wrt_resolve_file_module(importer, name);
  } else {
    resolved = name;
  }

  printf("Resolved: Importer: %s, Module: %s\n", importer, resolved);
  return resolved;
}

void wrt_run_main(WrenVM* vm, const char* main){
  const char* script = wrt_read_file(main);
  if(script == NULL) return;
  WrenInterpretResult result = wrenInterpret(vm, main, script);
  if(result == WREN_RESULT_SUCCESS){
    wrt_call_update_callbacks(vm);
  }
}

WrenVM* wrt_new_wren_vm(bool isMain){
  WrenConfiguration config;
  wrenInitConfiguration(&config);
  
  config.errorFn = error_fn;
  config.writeFn = write_fn;
  config.loadModuleFn = load_module_fn;
  config.bindForeignMethodFn = bind_method_fn;
  config.bindForeignClassFn = bind_class_fn;
  config.resolveModuleFn = resolve_module_fn;
  //config.initialHeapSize = 1024 * 1024 * 100;
  // config.minHeapSize = 1024 * 512;
  // config.heapGrowthPercent = 15;
  WrenVM* vm = wrenNewVM(&config);
  WrenUserData* ud = calloc(1, sizeof(WrenUserData)); 
  ud->isMainThread = isMain;
  wrenSetUserData(vm, (void*)ud);
  return vm;
}

void wrt_init(const char* mRoot){
  moduleRoot = mRoot;
  MUTEX_INIT(&mutex);
}