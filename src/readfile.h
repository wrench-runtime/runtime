#ifndef READFILE_H
#define READFILE_H

#include <stdio.h>
#include <stdlib.h>

static char *read_file_string(const char *filename)
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
  return buffer;
}

#endif