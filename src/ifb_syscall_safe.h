#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef __clang__
#else
#define restrict __restrict
#endif

// To ignore prgama warnings
// gcc -Wno-unknown-pragmas

#pragma region STDLIB_H

#pragma endregion

#pragma region STRING_H

/*
string.h -> strncpy()
These functions copy non-null bytes from the string pointed to  by  src
       into  the  array pointed to by dst.
*/
bool safe_strncpy(char *restrict dst, const char *restrict src,
                  size_t count) {
  if (dst == NULL || src == NULL) {
    return false;
  }

  strncpy(dst, src, count);
  return true;
}

#pragma endregion
