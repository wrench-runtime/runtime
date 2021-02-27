#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <cwalk.h>
#include <stdlib.h>
#include "modules.h"

const char* wrt_read_file(const char *filename)
{
  FILE* file = fopen(filename, "rb");
  if(file == NULL){
    printf("File not found %s\n", filename);
    return NULL;
  }
  long old = ftell(file);
  long numbytes;
  fseek(file, 0L, SEEK_END);
  numbytes = ftell(file);
  fseek(file, old, SEEK_SET);
  char* buffer = (char*)malloc((numbytes+1) * sizeof(char));	
  size_t read = fread(buffer, sizeof(char), numbytes, file);
  buffer[(int)read] = 0;
  fclose(file);
  return (const char*)buffer;
}

bool wrt_file_exists(const char* filename){
  FILE* file = fopen(filename, "rb");
  if(file == NULL){
    return false;
  } else {
    fclose(file);
    return true;
  }
}

static inline const char* copy_string(const char* original){
  const char* cpy = calloc(strlen(original)+1, sizeof(char));
  strcpy((char*)cpy, original);
  return cpy;
}

static inline const char* create_string(int size){
  const char* cpy = calloc(size+1, sizeof(char));
  return cpy;
}

bool wrt_is_file_module(const char* path){
  return path[0] == '.';
}

const char* wrt_resolve_file_module(const char* importer, const char* name){
  char* base = (char*)copy_string(importer);
  int length;
  cwk_path_get_dirname((const char*)base, &length);
  base[length] = 0;
  size_t result_size = length + strlen(name) + 5; // 5 for ".wren"
  const char* result = create_string(result_size);
  cwk_path_join(base, name, (char*)result, result_size);
  strcat((char*)result, ".wren");
  free(base);
  return result;
}

const char* wrt_resolve_installed_module(const char** locations, int num_locations, const char* name){
  char buffer[PATH_MAX];
  for (size_t i = 0; i < num_locations; i++)
  {
    int size = cwk_path_join(locations[i], name, buffer, sizeof(buffer));
    strcat(buffer, ".wren");
    if(wrt_file_exists((const char*)buffer)){
      const char* result = copy_string((const char*)buffer);
      return result;
    }
  }
  return name;
}

const char* wrt_resolve_binary_module(const char* path){
  const char* dll_path = copy_string(path);
  #if defined(_WIN32)
  cwk_path_change_extension(path, ".dll", (char*)dll_path, strlen(dll_path));
  #elif defined(__unix__)
  cwk_path_change_extension(path, ".so", dll_path, str_len(dll_path));
  #endif
  return dll_path;
}