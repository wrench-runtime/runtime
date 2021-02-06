//adapted from Boby Thomas pazheparampil - march 2006
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
  #include <windows.h>
#elif defined(__unix__)
  #include <dlfcn.h>
#else
  #error unsupported platform
#endif

#include "os_call.h"

#define RTLD_DEFAULT 0
#define RTLD_LAZY   1
#define RTLD_NOW    2
#define RTLD_GLOBAL 4

#if defined(_WIN32)
static BOOL PrintErrorMessage(DWORD dwErrorCode)
{
    LPTSTR pBuffer = (LPTSTR)malloc(sizeof(TCHAR)*1024);

    DWORD cchMsg = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL,  /* (not used with FORMAT_MESSAGE_FROM_SYSTEM) */
                                 dwErrorCode,
                                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                 pBuffer,
                                 1024,
                                 NULL);
    if(cchMsg > 0){
      printf("WINDOWS ERROR: %s\n", pBuffer);
    }

    free(pBuffer);
}
#endif

void* wrt_dlopen(const char *pcDllname)
{
  puts(pcDllname);
    #if defined(_WIN32)
      void* handle = (void*)LoadLibrary(pcDllname);
      if(handle == NULL) {
        printf("Error loading %s\n", pcDllname);
        PrintErrorMessage(GetLastError());
      }
      return handle;
    #elif defined(__GNUC__)
      void* handle = dlopen(pcDllname, RTLD_DEFAULT);
      if(handle == NULL) printf("Error loading %s. Reason %s\n", pcDllname, dlerror());
      printf("Loaded posix plugin %s\n", pcDllname);
      return handle;
    #else
      printf("Error: Native Plugin loading is not supported by platform\n");
    #endif
}

void *wrt_dlsym(void *Lib, char *Fnname)
{
  #if defined(_WIN32)
      return (void*)GetProcAddress((HINSTANCE)Lib,Fnname);
  #elif defined(__unix__) 
      return dlsym(Lib,Fnname);
  #endif
}

int wrt_dlclose(void *hDLL)
{
#if defined(_WIN32)
    return FreeLibrary((HINSTANCE)hDLL);
#elif defined(__unix__)
    return dlclose(hDLL);
#endif
}
