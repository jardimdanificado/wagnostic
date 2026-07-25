// assert.h — minimal libc shim for -nostdlib WASM builds

#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef NDEBUG
  #define assert(expr) ((void)0)
#else
  #define assert(expr) ((void)0)
#endif

#endif
