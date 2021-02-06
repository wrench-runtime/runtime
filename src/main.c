#include <stdio.h>
#include <assert.h>
#include <wren.h>
#include <string.h>

// CAUTION: Do this only once
#define STB_DS_IMPLEMENTATION
#include <stb_ds.h>

#include "os_call.h"
#include "mutex.h"
#include "readfile.h"

#define WRT_SEND_API(T) apiFunc(#T, T)
#ifdef DEBUG
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

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

static void wrt_set_plugin_data(WrenVM* vm, int handle, void* value){
  WrenUserData* ud = (WrenUserData*)wrenGetUserData(vm);
  if(ud->numPluginData < handle){
    ud->pluginData = ud->numPluginData > 0 ? realloc(ud->pluginData, handle*sizeof(void*)) : malloc(handle*sizeof(void*));
    ud->numPluginData = handle;
  }
  ud->pluginData[handle-1] = value;
}

static void* wrt_get_plugin_data(WrenVM* vm, int handle){
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

static void wrt_bind_method(const char* name, WrenForeignMethodFn func){
  shput(bindings, name, func);
}

static void wrt_bind_class(const char* name, WrenForeignMethodFn allocator, WrenFinalizerFn finalizer){
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

static void wrt_wren_update_callback(WrenForeignMethodFn fn){
  WrenCallbackNode* cb = malloc(sizeof(WrenCallbackNode));
  cb->callback = fn;
  cb->next = NULL;

  callbacks_push(&updateCallbacks, cb);
}

static void call_update_callbacks(WrenVM* vm) {
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

static void send_api(void (*apiFunc)(const char*, void*));

static int plugin_id = 1;


#define LoadPluginAssert(assert, msg) if(!(assert)){ puts(msg); goto DONE; }

static void load_plugin(WrenVM* vm, const char * in_name){
  MUTEX_LOCK(&mutex);
  BinaryModuleData md;
  
  int len = strlen(in_name);
  char* name = malloc(sizeof(char)*len+1);
  strcpy(name, in_name);

  int index = shgeti(binaryModules, name);
  if(index == -1){
    FILE* f = fopen(name, "rb");
    
    if(f == NULL) goto DONE;
    else fclose(f);
    puts(name);
    void* handle = wrt_dlopen(name);
    LoadPluginAssert(handle != NULL, "Could not open binary plugin");

    void (*initFunc)(int handle) = wrt_dlsym(handle, "wrt_plugin_init");
    LoadPluginAssert(initFunc != NULL, "Did not find init entry point in binary plugin");
    void (*apiFunc)(const char*, void*) = wrt_dlsym(handle, "wrt_plugin_api");
    LoadPluginAssert(apiFunc != NULL, "Did not find api load entry point in binary plugin");

    send_api(apiFunc);
    initFunc(plugin_id++);
    md.wrenInitFunc = wrt_dlsym(handle, "wrt_plugin_init_wren");
    shput(binaryModules, name, md);
  }
  else {
    free(name);
    md = binaryModules[index].value;
  }

  if(md.wrenInitFunc != NULL){
    void (*wrenInitFunc)(WrenVM*) = md.wrenInitFunc;
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

  char strbuffer[1024];
  char dllbuffer[1024];

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
    load_plugin(vm, dllbuffer);
    strcat(strbuffer, ".wren");
  }

  const char * string = read_file_string(strbuffer);

  //result.onComplete = loadModuleComplete;
  result.source = string;
  return result;
}

static WrenVM* wrt_new_wren_vm(){
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
  wrenSetUserData(vm, (void*)ud);
  return vm;
}

static void send_api(void (*apiFunc)(const char*, void*)){
  WRT_SEND_API(wrenGetSlotCount);
  WRT_SEND_API(wrenEnsureSlots);
  WRT_SEND_API(wrenGetSlotType);
  WRT_SEND_API(wrenGetSlotBool);
  WRT_SEND_API(wrenGetSlotBytes);
  WRT_SEND_API(wrenGetSlotDouble);
  WRT_SEND_API(wrenGetSlotForeign);
  WRT_SEND_API(wrenGetSlotString);
  WRT_SEND_API(wrenGetSlotHandle);
  WRT_SEND_API(wrenSetSlotBool);
  WRT_SEND_API(wrenSetSlotBytes);
  WRT_SEND_API(wrenSetSlotDouble);
  WRT_SEND_API(wrenSetSlotNewForeign);
  WRT_SEND_API(wrenSetSlotNewList);
  WRT_SEND_API(wrenSetSlotNewMap);
  WRT_SEND_API(wrenSetSlotNull);
  WRT_SEND_API(wrenSetSlotString);
  WRT_SEND_API(wrenSetSlotHandle);
  WRT_SEND_API(wrenGetListCount);
  WRT_SEND_API(wrenGetListElement);
  WRT_SEND_API(wrenInsertInList);
  WRT_SEND_API(wrenGetMapCount);
  WRT_SEND_API(wrenGetMapContainsKey);
  WRT_SEND_API(wrenGetMapValue);
  WRT_SEND_API(wrenSetMapValue);
  WRT_SEND_API(wrenRemoveMapValue);
  WRT_SEND_API(wrenReleaseHandle);
  WRT_SEND_API(wrenAbortFiber);
  WRT_SEND_API(wrenMakeCallHandle);
  WRT_SEND_API(wrenCall);
  WRT_SEND_API(wrenGetVariable);
  WRT_SEND_API(wrenInterpret);

  WRT_SEND_API(wrt_bind_class);
  WRT_SEND_API(wrt_bind_method);
  WRT_SEND_API(wrt_wren_update_callback);
  WRT_SEND_API(wrt_set_plugin_data);
  WRT_SEND_API(wrt_get_plugin_data);
  WRT_SEND_API(wrt_new_wren_vm);
}

int main(int argc, char *argv[])
{  
  MUTEX_INIT(&mutex);

  WrenVM* vm = wrt_new_wren_vm(); 
  ((WrenUserData*)wrenGetUserData(vm))->isMainThread = true;

  char* script;
  if(argc < 2){
    script = read_file_string("main.wren");
  }
  else {
    script = read_file_string(argv[1]);
  }
  assert(script != NULL);
  WrenInterpretResult result = wrenInterpret(vm, "main", script);
  free(script);
  if(result == WREN_RESULT_SUCCESS){
    while(updateCallbacks.start != NULL){
      call_update_callbacks(vm);
    }
  }
  return 0;
}
