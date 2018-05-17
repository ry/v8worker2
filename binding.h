/*
Copyright 2018 Ryan Dahl <ry@tinyclouds.org>. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
*/
#ifndef BINDING_H
#define BINDING_H
#ifdef __cplusplus
extern "C" {
#endif

struct worker_s;
typedef struct worker_s worker;

struct buf_s {
  void* data;
  size_t len;
};
typedef struct buf_s buf;

const char* worker_version();
void worker_set_flags(int* argc, char** argv);

void v8_init();

worker* worker_new(int table_index);

// returns nonzero on error
// get error from worker_last_exception
int worker_load(worker* w, char* name_s, char* source_s);
int worker_load_module(worker* w, char* name_s, char* source_s, int callback_index);

const char* worker_last_exception(worker* w);

int worker_send_bytes(worker* w, void* data, size_t len);

void worker_dispose(worker* w);
void worker_terminate_execution(worker* w);

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // BINDING_H
