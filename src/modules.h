#ifndef WRT_MODULES_H
#define WRT_MODULES_H

bool wrt_file_exists(const char* filename);
const char* wrt_read_file(const char *filename);
bool wrt_is_file_module(const char* path);
const char* wrt_resolve_file_module(const char* importer, const char* name);
const char* wrt_resolve_binary_module(const char* path);
const char* wrt_resolve_installed_module(const char** locations, int num_locations, const char* name);


#endif