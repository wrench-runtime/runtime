//adapted from Boby Thomas pazheparampil - march 2006
#ifndef os_call_h
#define os_call_h

void* wrt_dlopen(const char *pcDllname);
void *wrt_dlsym(void *Lib, char *Fnname);
int wrt_dlclose(void *hDLL);

#endif //os_call_h