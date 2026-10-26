#pragma once
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_HEADERS 32

typedef struct {
  uint16_t off;
  uint16_t len;
} HttpValue;

typedef struct {
  HttpValue key;
  HttpValue value;
} HttpHeader;

typedef struct {
  HttpValue method;
  HttpValue url;
  HttpValue version;

  HttpHeader headers[MAX_HEADERS];
  uint16_t num_headers;
  uint16_t content_start;
} HttpRequest;

/// Prerequisite before starting to parse any http request.
///
bool has_2crlf(const char* data, int len) {
  if (len < 4) {
    return false;
  }
  int ix = 3;
  while (ix < len) {
    switch (data[ix]) {
      case '\n':
        if (memcmp(&data[ix-3], "\r\n\r\n", 4) == 0)
          return true;
        ix += 2;
        break;
      case '\r':
        ix += 1;
        break;
      default:
        ix += 4;
        break;
    }
  }
  return false;
}

int HttpRequest_parse(const char* buffer, int len, HttpRequest* request) {
  // Minimal valid request: 'GET / HTTP/1.1\r\n\r\n' (18 bytes)
  if (len < 18) {
    return -1;
  }
  if (!has_2crlf(buffer, len)) {
    return -1;
  }

  int ix = 0;
  HttpRequest result = {};

  result.method.off = ix;
  for (;;) {
    if (ix == len) {
      return -1;
    }

    if (buffer[ix] == ' ') {
      break;
    }
    ix++;
  }
  result.method.len = ix - result.method.off;
  ix++;

  result.url.off = ix;
  for (;;) {
    if (ix == len) {
      return -1;
    }

    if (buffer[ix] == ' ') {
      break;
    }
    ix++;
  }
  result.url.len = ix - result.url.off;
  ix++;

  result.version.off = ix;
  for (;;) {
    if (ix == len) {
      return -1;
    }

    if (buffer[ix] == '\r') {
      break;
    }
    ix++;
  }
  result.version.len = ix - result.version.off;
  ix += 2;

  result.num_headers = 0;
  for (;;) {
    if (ix >= len) {
      return -1;
    }

    if (buffer[ix] == '\r') {
      break;
    }

    if (result.num_headers >= MAX_HEADERS) {
      return -1;
    }

    HttpValue key;
    HttpValue value;

    key.off = ix;
    for (;;) {
      if (ix >= len) {
        return -1;
      }

      if (buffer[ix] == ':') {
        break;
      }
      ix++;
    }
    key.len = ix - key.off;
    ix++;

    for (;;) {
      if (ix >= len) {
        return -1;
      }
      if (buffer[ix] != ' ' && buffer[ix] != '\t') {
        break;
      }
      ix++;
    }
    ix++;

    value.off = ix;
    for (;;) {
      if (ix >= len) {
        return -1;
      }

      if (buffer[ix] == '\r') {
        break;
      }
      ix++;
    }
    value.len = ix - value.off;
    ix += 2;

    result.headers[result.num_headers] = (HttpHeader){ .key = key, .value = value };
    result.num_headers++;
  }
  ix += 2;

  result.content_start = ix;
  
  *request = result;
  return 0;
}
