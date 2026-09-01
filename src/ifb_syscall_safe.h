#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#ifndef __clang__
#else
#define restrict __restrict
#endif

// To ignore prgama warnings
// gcc -Wno-unknown-pragmas

#pragma region SAFE_UTILS

// protocol: 2 = AF_INET (IPv4) , 10 = AF_INET6 (IPv6)
bool check_host_error(const char *hostname, uint8_t protocol, char *err_buf,
                      size_t err_buf_size) {

  if (protocol != 2 && protocol != 10) {
    snprintf(err_buf, err_buf_size,
             "invalid protocol value, should be 2 (AF_INET) or 10 (AF_INET6), "
             "actual: %d\n",
             protocol);
    return false;
  }

  struct addrinfo hints = {0};
  struct addrinfo *res = NULL;

  int err;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = (int)protocol;

  err = getaddrinfo(hostname, NULL, &hints, &res);
  if (err != 0) {
    snprintf(err_buf, err_buf_size, "DNS error: %s", gai_strerror(err));
    return false;
  }

  freeaddrinfo(res);
  return true;
}

#pragma endregion

#pragma region STDLIB_H

#pragma endregion

#pragma region STRING_H

/*
string.h -> strncpy()
These functions copy non-null bytes from the string pointed to  by  src
       into  the  array pointed to by dst.
*/
bool safe_strncpy(char *restrict dst, const char *restrict src, size_t count) {
  if (dst == NULL || src == NULL) {
    return false;
  }

  strncpy(dst, src, count);
  return true;
}

#pragma endregion
