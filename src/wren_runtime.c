#include <stdio.h>
#include <assert.h>
#include <wren.h>
#include <string.h>

#include <wren_runtime.h>

#include "readfile.h"

// CAUTION: Do this only once
#define STB_DS_IMPLEMENTATION
#include <stb_ds.h>

#include "os_call.h"
#include "mutex.h"

MUTEX mutex;

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

static WrenForeignMethodFn bindMethodFunc( 
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

WrenForeignClassMethods bindClassFunc( 
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
    FILE* f = fopen(name, "rb");
    
    if(f == NULL) goto DONE;
    else fclose(f);
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

  if(strcmp(name, "random") == 0 || strcmp(name, "meta") == 0){
    return result;
  }

  char strbuffer[4096];
  char dllbuffer[4096];

  //file
  if(name[0] == '.'){
    strcpy(strbuffer, name);
    strcat(strbuffer, ".wren");
  }
  //module
  else {
    strcpy(strbuffer, "./wren_modules/");
    strcat(strbuffer, name);
    strcpy(dllbuffer, strbuffer);
    #if defined(_WIN32)
    strcat(dllbuffer, ".dll");
    #elif defined(__EMSCRIPTEN__)
    strcat(dllbuffer, ".wasm");
    #elif defined(__unix__)
    strcat(dllbuffer, ".so");
    #endif
    load_plugin(vm, name, dllbuffer);
    strcat(strbuffer, ".wren");
  }

  const char * string = read_file_string(strbuffer);

  //result.onComplete = loadModuleComplete;
  result.source = string;
  return result;
}

WrenVM* wrt_new_wren_vm(bool isMain){
  WrenConfiguration config;
  wrenInitConfiguration(&config);
  
  config.errorFn = error_fn;
  config.writeFn = write_fn;
  config.loadModuleFn = load_module_fn;
  config.bindForeignMethodFn = bindMethodFunc;
  config.bindForeignClassFn = bindClassFunc;
  //config.initialHeapSize = 1024 * 1024 * 100;
  // config.minHeapSize = 1024 * 512;
  // config.heapGrowthPercent = 15;
  WrenVM* vm = wrenNewVM(&config);
  WrenUserData* ud = calloc(1, sizeof(WrenUserData)); 
  ud->isMainThread = isMain;
  wrenSetUserData(vm, (void*)ud);
  return vm;
}

void wrt_init(){
  MUTEX_INIT(&mutex);
}